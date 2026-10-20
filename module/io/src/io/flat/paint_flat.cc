// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <skity/graphic/paint.hpp>

#include "src/io/core_bridge.hpp"
#include "src/io/flat/blender_flat.hpp"
#include "src/io/flat/local_matrix_flat.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"
#include "src/picture.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

constexpr uint8_t CUSTOM_BLEND_MODE_SENTINEL = 0xFF;

// paint flat flags
enum FlatFlags {
  kHasTypeface_FlatFlag = 0x1,
  kHasEffects_FlatFlag = 0x2,

  kFlatFlagMask = 0x3,
};

template <typename T>
uint32_t shift_bits(T value, unsigned shift, unsigned bits) {
  uint32_t v = static_cast<uint32_t>(value);
  return v << shift;
}

/**
 * Packing the paint
 * flags :  8  // 2...
 * blend :  8  // 30+
 * cap   :  2  // 3
 * join  :  2  // 3
 * style :  2  // 3
 * filter:  2  // 4
 * flat  :  8  // 1...
 * total : 32
 */
uint32_t pack_v68(const Paint& paint, uint8_t flat_flags) {
  uint32_t packed = 0;

  auto bm = static_cast<uint32_t>(core_bridge::GetPaintBlendMode(&paint));

  // ignore dither
  // we can ignore isAntiAlias as well, but this is meaningful for skia
  packed = shift_bits(core_bridge::IsPaintAntiAlias(&paint) ? 1u : 0u, 0, 8);

  packed |= shift_bits(bm, 8, 8);  // blend mode
  packed |= shift_bits(
      static_cast<uint32_t>(core_bridge::GetPaintStrokeCap(&paint)), 16, 2);
  packed |= shift_bits(
      static_cast<uint32_t>(core_bridge::GetPaintStrokeJoin(&paint)), 18, 2);
  packed |= shift_bits(
      static_cast<uint32_t>(core_bridge::GetPaintStyle(&paint)), 20, 2);
  packed |= shift_bits(0, 22, 2);
  packed |= shift_bits(flat_flags, 24, 8);

  return packed;
}

uint32_t unpack_v68(const std::shared_ptr<Paint>& paint, uint32_t packed) {
  core_bridge::SetPaintAntiAlias(paint, (packed & 1) != 0);

  // skip dither

  packed >>= 8;
  {
    uint32_t mode = packed & 0xFF;

    if (mode != CUSTOM_BLEND_MODE_SENTINEL) {
      skity_paint handle = skity::capi::alloc_handle<skity_paint_s>(
          SKITY_OBJECT_TYPE_PAINT, SKITY_HANDLE_OWNING, paint);
      if (handle != nullptr) {
        skity_paint_set_blend_mode(
            handle, static_cast<skity_blend_mode>(std::min(
                        mode, static_cast<uint32_t>(BlendMode::kLastMode))));
        skity_paint_destroy(handle);
      }
    }
  }

  packed >>= 8;
  core_bridge::SetPaintStrokeCap(paint, static_cast<Paint::Cap>(packed & 0x3));
  packed >>= 2;
  core_bridge::SetPaintStrokeJoin(paint,
                                  static_cast<Paint::Join>(packed & 0x3));
  packed >>= 2;
  core_bridge::SetPaintStyle(paint, static_cast<Paint::Style>(packed & 0x3));
  packed >>= 2;
  // skip the fiter quality bits
  packed >>= 2;

  return packed;
}

}  // namespace

template <>
void FlatIntoMemory<Paint>(const Paint& paint, WriteBuffer& writer) {
  uint8_t flat_flags = 0;

  if (paint.GetPathEffect() || paint.GetShader() || paint.GetMaskFilter() ||
      paint.GetColorFilter() || paint.GetImageFilter() ||
      core_bridge::GetPaintBlendMode(&paint) != SKITY_BLEND_MODE_SRC_OVER) {
    flat_flags |= kHasEffects_FlatFlag;
  }

  writer.WriteFloat(core_bridge::GetPaintStrokeWidth(&paint));
  writer.WriteFloat(core_bridge::GetPaintStrokeMiter(&paint));

  auto color = core_bridge::GetPaintColor4f(&paint);
  writer.WriteColor4f(color);

  writer.WriteUint32(pack_v68(paint, flat_flags));

  if (flat_flags & kHasEffects_FlatFlag) {
    writer.WriteFlattenable(paint.GetPathEffect().get());  // path effect

    // shader
    if (paint.GetShader()) {
      const auto& shader = paint.GetShader();
      auto matrix = shader->GetLocalMatrix();

      if (core_bridge::MatrixIsIdentity(matrix)) {
        writer.WriteFlattenable(shader.get());
      } else {
        LocalMatrixFlat local_matrix_flat(matrix, shader.get());
        writer.WriteFlattenable(&local_matrix_flat);
      }
    } else {
      writer.WriteFlattenable(nullptr);
    }

    writer.WriteFlattenable(paint.GetMaskFilter().get());   // mask filter
    writer.WriteFlattenable(paint.GetColorFilter().get());  // color filter
    writer.WriteFlattenable(paint.GetImageFilter().get());  // image filter

    if (paint.GetBlendMode() != BlendMode::kSrcOver) {
      // other blend mode will generate a SkBlendModeBlender instance in skia
      // this is a Flattentable instance so we need to write it
      BlenderModeFlattenable flattenable(paint.GetBlendMode());
      writer.WriteFlattenable(&flattenable);
    } else {
      writer.WriteFlattenable(nullptr);
    }
  }
}

template <>
std::shared_ptr<Paint> ReadFromMemory(ReadBuffer& buffer) {
  auto paint = core_bridge::MakePaint();

  if (!paint) {
    return nullptr;
  }

  core_bridge::SetPaintStrokeWidth(paint, buffer.ReadFloat());
  core_bridge::SetPaintStrokeMiter(paint, buffer.ReadFloat());

  {
    auto color4f = buffer.ReadColor4f();
    core_bridge::SetPaintColor(paint, core_bridge::Color4fToColor(color4f));
  }

  auto packed = buffer.ReadU32();

  auto flat_flags = unpack_v68(paint, packed);

  if (!(flat_flags & kHasEffects_FlatFlag)) {
    core_bridge::SetPaintPathEffect(paint, nullptr);
    core_bridge::SetPaintShader(paint, nullptr);
    core_bridge::SetPaintMaskFilter(paint, nullptr);
    core_bridge::SetPaintColorFilter(paint, nullptr);
    core_bridge::SetPaintImageFilter(paint, nullptr);
  } else if (buffer.IsVersionLT(kSkBlenderInSkPaint)) {
    core_bridge::SetPaintPathEffect(paint, buffer.ReadPathEffect());
    core_bridge::SetPaintShader(paint, buffer.ReadShader());
    core_bridge::SetPaintMaskFilter(paint, buffer.ReadMaskFilter());
    core_bridge::SetPaintColorFilter(paint, buffer.ReadColorFilter());
    (void)buffer.ReadU32();  // old version property deprecated
    core_bridge::SetPaintImageFilter(paint, buffer.ReadImageFilter());
  } else {
    core_bridge::SetPaintPathEffect(paint, buffer.ReadPathEffect());
    core_bridge::SetPaintShader(paint, buffer.ReadShader());
    core_bridge::SetPaintMaskFilter(paint, buffer.ReadMaskFilter());
    core_bridge::SetPaintColorFilter(paint, buffer.ReadColorFilter());
    core_bridge::SetPaintImageFilter(paint, buffer.ReadImageFilter());

    // BlenderModeFlattenable::SkipReadBlender(buffer);
    buffer.SkipBlender();
  }

  if (!buffer.IsValid()) {
    return nullptr;
  }

  return paint;
}

}  // namespace skity
