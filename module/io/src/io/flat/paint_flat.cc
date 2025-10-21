// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cmath>
#include <skity/graphic/paint.hpp>
#include <skity/io/picture.hpp>

#include "src/io/flat/blender_flat.hpp"
#include "src/io/flat/local_matrix_flat.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"
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

  auto bm = static_cast<uint32_t>(paint.GetBlendMode());

  // ignore dither
  // we can ignore isAntiAlias as well, but this is meaningful for skia
  packed = shift_bits(static_cast<uint32_t>(paint.IsAntiAlias()), 0, 8);

  packed |= shift_bits(bm, 8, 8);  // blend mode
  packed |= shift_bits(paint.GetStrokeCap(), 16, 2);
  packed |= shift_bits(paint.GetStrokeJoin(), 18, 2);
  packed |= shift_bits(paint.GetStyle(), 20, 2);
  packed |= shift_bits(0, 22, 2);
  packed |= shift_bits(flat_flags, 24, 8);

  return packed;
}

uint32_t unpack_v68(Paint& paint, uint32_t packed) {
  paint.SetAntiAlias((packed & 1) != 0);

  // skip dither

  packed >>= 8;
  {
    uint32_t mode = packed & 0xFF;

    if (mode != CUSTOM_BLEND_MODE_SENTINEL) {
      paint.SetBlendMode(static_cast<BlendMode>(
          std::min(mode, static_cast<uint32_t>(BlendMode::kLastMode))));
    }
  }

  packed >>= 8;
  paint.SetStrokeCap(static_cast<Paint::Cap>(packed & 0x3));
  packed >>= 2;
  paint.SetStrokeJoin(static_cast<Paint::Join>(packed & 0x3));
  packed >>= 2;
  paint.SetStyle(static_cast<Paint::Style>(packed & 0x3));
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
      paint.GetBlendMode() != BlendMode::kSrcOver) {
    flat_flags |= kHasEffects_FlatFlag;
  }

  writer.WriteFloat(paint.GetStrokeWidth());
  writer.WriteFloat(paint.GetStrokeMiter());

  auto color = paint.GetColor4f();
  writer.WriteColor4f(color);

  writer.WriteUint32(pack_v68(paint, flat_flags));

  if (flat_flags & kHasEffects_FlatFlag) {
    writer.WriteFlattenable(paint.GetPathEffect().get());  // path effect

    // shader
    if (paint.GetShader()) {
      const auto& shader = paint.GetShader();
      auto matrix = shader->GetLocalMatrix();

      if (matrix.IsIdentity()) {
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
std::optional<Paint> ReadFromMemory(ReadBuffer& buffer) {
  Paint paint;

  paint.SetStrokeWidth(buffer.ReadFloat());
  paint.SetStrokeMiter(buffer.ReadFloat());

  {
    auto color4f = buffer.ReadColor4f();
    paint.SetColor(Color4fToColor(color4f));
  }

  auto packed = buffer.ReadU32();

  auto flat_flags = unpack_v68(paint, packed);

  if (!(flat_flags & kHasEffects_FlatFlag)) {
    paint.SetPathEffect(nullptr);
    paint.SetShader(nullptr);
    paint.SetMaskFilter(nullptr);
    paint.SetColorFilter(nullptr);
    paint.SetImageFilter(nullptr);
  } else if (buffer.IsVersionLT(kSkBlenderInSkPaint)) {
    paint.SetPathEffect(buffer.ReadPathEffect());
    paint.SetShader(buffer.ReadShader());
    paint.SetMaskFilter(buffer.ReadMaskFilter());
    paint.SetColorFilter(buffer.ReadColorFilter());
    (void)buffer.ReadU32();  // old version property deprecated
    paint.SetImageFilter(buffer.ReadImageFilter());
  } else {
    paint.SetPathEffect(buffer.ReadPathEffect());
    paint.SetShader(buffer.ReadShader());
    paint.SetMaskFilter(buffer.ReadMaskFilter());
    paint.SetColorFilter(buffer.ReadColorFilter());
    paint.SetImageFilter(buffer.ReadImageFilter());

    BlenderModeFlattenable::SkipReadBlender(buffer);
  }

  if (!buffer.IsValid()) {
    return std::nullopt;
  }

  return paint;
}

}  // namespace skity
