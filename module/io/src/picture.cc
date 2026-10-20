// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/picture.hpp"

#include <skity_c/skity_recorder.h>

#include <array>
#include <cstring>

#include "src/io/core_bridge.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"
#include "src/picture_priv.hpp"
#include "src/record/record_playback.hpp"

namespace skity {

namespace {

template <typename T>
static constexpr bool IsAlign4(T x) {
  return 0 == (x & 3);
}

// Borrowed-handle constructors for replaying recorded objects through the
// canvas C ABI. Each wrapper views the referenced object without owning it;
// the C entry point only reads it synchronously.
skity_path wrap_path(const Path& path) {
  std::shared_ptr<Path> impl(const_cast<Path*>(&path), [](Path*) {});
  return skity::capi::alloc_handle<skity_path_s>(SKITY_OBJECT_TYPE_PATH, 0,
                                                 std::move(impl));
}

skity_paint wrap_paint(const Paint* paint) {
  if (paint == nullptr) {
    return nullptr;
  }
  std::shared_ptr<Paint> impl(const_cast<Paint*>(paint), [](Paint*) {});
  return skity::capi::alloc_handle<skity_paint_s>(SKITY_OBJECT_TYPE_PAINT, 0,
                                                  std::move(impl));
}

skity_text_blob wrap_blob(const std::shared_ptr<TextBlob>& blob) {
  if (blob == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, SKITY_HANDLE_OWNING, blob);
}

skity_image wrap_image(const std::shared_ptr<Image>& image) {
  if (image == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_image_s>(SKITY_OBJECT_TYPE_IMAGE,
                                                  SKITY_HANDLE_OWNING, image);
}

skity_sampling_options to_sampling(const SamplingOptions& so) {
  skity_sampling_options out{};
  out.filter = static_cast<skity_filter_mode>(so.filter);
  out.mipmap = static_cast<skity_mipmap_mode>(so.mipmap);
  out.cubic_b = so.cubic.B;
  out.cubic_c = so.cubic.C;
  return out;
}

struct PictureInfo {
  std::array<char, 8> magic = {'s', 'k', 'i', 'a', 'p', 'i', 'c', 't'};
  uint32_t version = 0;
  Rect cull_rect;
};

bool is_valid_picture(const PictureInfo& info) {
  if (info.magic !=
      std::array<char, 8>{'s', 'k', 'i', 'a', 'p', 'i', 'c', 't'}) {
    return false;
  }

  if (info.version < Version::kMin_Version ||
      info.version > Version::kCurrent_Version) {
    return false;
  }

  return true;
}

PictureInfo create_header(const Rect& cull_rect) {
  PictureInfo info;

  info.version = Version::kCurrent_Version;
  info.cull_rect = cull_rect;

  return info;
}

bool parse_header(ReadStream& stream, PictureInfo& info) {
  PictureInfo temp;

  if (stream.Read(&temp.magic, sizeof(temp.magic)) != sizeof(temp.magic)) {
    return false;
  }

  if (!stream.ReadU32(&temp.version)) {
    return false;
  }

  float left, top, right, bottom;

  if (!stream.ReadFloat(&left) || !stream.ReadFloat(&top) ||
      !stream.ReadFloat(&right) || !stream.ReadFloat(&bottom)) {
    return false;
  }

  temp.cull_rect = Rect::MakeLTRB(left, top, right, bottom);

  info = temp;

  return is_valid_picture(info);
}

float sigma_to_radius(float sigma) {
  return sigma > 0.5f ? (sigma - 0.5f) / 0.57735f : 0.0f;
}

static constexpr auto kAmbientHeightFactor = 1.0f / 128.0f;
static constexpr auto kAmbientGeomFactor = 64.0f;

static constexpr auto kMaxAmbientRadius =
    300 * kAmbientHeightFactor * kAmbientGeomFactor;

float ambient_blur_radius(float z) {
  return std::min(z * kAmbientHeightFactor * kAmbientGeomFactor,
                  kMaxAmbientRadius);
}

float ambient_recip_alpha(float z) {
  return 1.0f + std::max(z * kAmbientHeightFactor, 0.f);
}

bool get_spot_shadow_transform(const Vec3& light_pos, float light_radius,
                               const Matrix& ctm, const Vec3& z_plane,
                               const Rect& bounds, bool directional,
                               Matrix& shadow_matrix, float& radius) {
  auto height_func = [z_plane](float x, float y) {
    return z_plane.x * x + z_plane.y * y + z_plane.z;
  };

  auto occluder_height = height_func(core_bridge::RectCenterX(bounds),
                                     core_bridge::RectCenterY(bounds));

  if (core_bridge::MatrixHasPersp(ctm)) {  // dose not support it for now
    return false;
  }

  float scale;
  Vec2 translate;
  if (directional) {
    radius = light_radius * occluder_height;
    scale = 1.f;
    constexpr float kMaxZRatio = 64 / kNearlyZero;

    auto z_ratio = std::clamp(occluder_height / light_pos.z, 0.f, kMaxZRatio);
    translate = Vec2{-z_ratio * light_pos.x, -z_ratio * light_pos.y};
  } else {
    auto z_ratio = std::clamp(occluder_height / (light_pos.z - occluder_height),
                              0.f, 0.95f);

    radius = light_radius * z_ratio;

    scale = std::clamp(occluder_height / (light_pos.z - occluder_height), 1.f,
                       1.95f);

    translate = Vec2{-z_ratio * light_pos.x, -z_ratio * light_pos.y};
  }

  shadow_matrix.SetScaleX(scale);
  shadow_matrix.SetScaleY(scale);
  shadow_matrix.SetTranslateX(translate.x);
  shadow_matrix.SetTranslateY(translate.y);

  return true;
}

}  // namespace

int32_t TypefaceSet::AddTypeface(const std::shared_ptr<Typeface>& typeface) {
  for (size_t i = 0; i < typefaces.size(); i++) {
    if (typefaces[i] == typeface) {
      return static_cast<int32_t>(i + 1);
    }
  }

  typefaces.emplace_back(typeface);

  return static_cast<int32_t>(typefaces.size());
}

int32_t FactorySet::AddFactory(const std::string& factory) {
  for (size_t i = 0; i < factories.size(); i++) {
    if (factories[i] == factory) {
      return static_cast<int32_t>(i + 1);
    }
  }

  factories.emplace_back(factory);

  return static_cast<int32_t>(factories.size());
}

std::string FactorySet::GetFactoryName(int32_t index) const {
  if (index < 0 || index >= factories.size()) {
    return {};
  }

  return factories[index];
}

size_t FactorySet::GetFactoryCount() const { return factories.size(); }

Picture::Picture(std::unique_ptr<RecordPlayback> playback)
    : playback_(std::move(playback)), writer_(new MemoryWriter32) {}

Picture::~Picture() {}

std::unique_ptr<Picture> Picture::MakeFromDisplayList(DisplayList* dl) {
  if (!dl || dl->Empty()) {
    return {};
  }

  auto rect = dl->GetBounds();
  auto playback = std::make_unique<RecordPlayback>(rect.Width(), rect.Height());

  // Replay the display list through the C ABI onto the recorder's delegate
  // canvas; the op stream re-enters RecordPlayback through its callbacks.
  std::shared_ptr<DisplayList> borrowed(dl, [](DisplayList*) {});
  skity_display_list list = skity::capi::alloc_handle<skity_display_list_s>(
      SKITY_OBJECT_TYPE_DISPLAY_LIST, 0, std::move(borrowed));
  if (list != nullptr) {
    skity_display_list_draw(list, playback->GetDelegateCanvas());
    skity_display_list_destroy(list);
  }

  std::unique_ptr<Picture> picture(new Picture(std::move(playback)));
  picture->cull_rect_ = rect;

  return picture;
}

std::unique_ptr<Picture> Picture::MakeFromStream(ReadStream& stream) {
  static constexpr int32_t kDefaultRecursionLimit = 100;

  return MakeFromStream(stream, nullptr, kDefaultRecursionLimit);
}

void Picture::Serialize(WriteStream& stream, const SerialProc* proc,
                        TypefaceSet* typeface_set) {
  auto info = create_header(cull_rect_);

  stream.Write(&info, sizeof(info));

  if (playback_) {
    stream.WriteU8(kPictureData_TrailingStreamByteAfterPictInfo);
    playback_->Serialize(stream, proc, typeface_set);
  } else {
    stream.WriteU8(kFailure_TrailingStreamByteAfterPictInfo);
  }
}

void Picture::PlayBack(Canvas* canvas) {
  if (canvas == nullptr) {
    return;
  }
  // The C-ABI playback path takes a handle; wrap the caller's canvas
  // (non-owning) for the duration of the replay.
  std::shared_ptr<Canvas> canvas_view(canvas, [](Canvas*) {});
  skity_canvas canvas_handle = skity::capi::alloc_handle<skity_canvas_s>(
      SKITY_OBJECT_TYPE_CANVAS, 0, std::move(canvas_view));

  ReadBuffer buffer(playback_->GetOpData()->RawData(),
                    playback_->GetOpData()->Size());

  buffer.SetVersion(playback_->GetTargetVersion());

  Matrix matrix{};
  skity_canvas_get_total_matrix(canvas_handle,
                                reinterpret_cast<skity_matrix*>(&matrix));

  auto restore = skity_canvas_save(canvas_handle);

  size_t offset = 0;
  while (!buffer.IsEOF() && buffer.IsValid()) {
    offset = buffer.GetOffset();

    auto bits = buffer.ReadU32();

    auto op = bits >> 24;
    auto size = bits & 0xFFFFFF;

    if (size == 0xFFFFFF) {
      size = buffer.ReadU32();
    }

    if (!buffer.Validate(size > 0 && op > UNUSED && op <= LAST_DRAWTYPE_ENUM)) {
      skity_canvas_restore_to_count(canvas_handle, restore);
      skity_canvas_destroy(canvas_handle);
      return;
    }

    HandleOp(buffer, static_cast<DrawType>(op), size, canvas_handle, matrix);
  }

  skity_canvas_restore_to_count(canvas_handle, restore);
  skity_canvas_destroy(canvas_handle);
}

std::unique_ptr<Picture> Picture::MakeFromStream(ReadStream& stream,
                                                 TypefaceSet* typeface_set,
                                                 int32_t recursion_limit) {
  if (recursion_limit <= 0) {
    return {};
  }

  PictureInfo info;

  if (!parse_header(stream, info)) {
    return {};
  }

  uint8_t trailing_stream_byte_after_pict_info;

  if (!stream.ReadU8(&trailing_stream_byte_after_pict_info)) {
    return {};
  }

  // we only support kPictureData_TrailingStreamByteAfterPictInfo
  if (trailing_stream_byte_after_pict_info !=
      kPictureData_TrailingStreamByteAfterPictInfo) {
    return {};
  }

  auto playback = RecordPlayback::CreateFromStream(
      info.cull_rect, info.version, stream, typeface_set, recursion_limit);

  if (!playback) {
    return {};
  }

  auto picture = std::unique_ptr<Picture>{new Picture(std::move(playback))};

  picture->cull_rect_ = info.cull_rect;

  return picture;
}

void SkipPictureInBuffer(ReadBuffer& buffer) {
  const PictureInfo* info =
      reinterpret_cast<const PictureInfo*>(buffer.Skip(sizeof(PictureInfo)));

  if (!buffer.Validate(is_valid_picture(*info))) {
    return;
  }

  // 0, 1 or negative
  int32_t ssize = buffer.ReadInt();

  if (ssize < 0) {
    size_t size = -ssize;
    (void)buffer.Skip(size);

    return;
  }

  RecordPlayback playback(info->cull_rect.Width(), info->cull_rect.Height(),
                          info->version);

  playback.ParseBuffer(buffer);
}

void Picture::HandleOp(ReadBuffer& buffer, uint32_t type, size_t size,
                       skity_canvas canvas, const Matrix& current_matrix) {
  auto op = static_cast<DrawType>(type);

  auto validate_offset = [&buffer](size_t offset) {
    if (offset) {
      buffer.Validate(IsAlign4(offset) && offset >= buffer.GetOffset());
    }
  };

#define BREAK_IF_ERROR(buf) \
  if (!buf.IsValid()) break /* NOLINT */

  switch (op) {
    case DrawType::NOOP:
      buffer.Skip(size - 4);
      break;
    case DrawType::FLUSH:
      break;
    case DrawType::CLIP_PATH: {
      const auto& path = playback_->GetPath(buffer);
      auto packed = buffer.ReadU32();

      auto un_packed_clip_op = packed & 0xF;

      auto offset_to_restore = buffer.ReadInt();
      validate_offset(offset_to_restore);
      BREAK_IF_ERROR(buffer);

      skity_clip_op clip_op = SKITY_CLIP_OP_INTERSECT;

      if (un_packed_clip_op == 0) {
        clip_op = SKITY_CLIP_OP_DIFFERENCE;
      }

      skity_canvas_clip_path(canvas, wrap_path(path), clip_op);
    } break;
    case DrawType::CLIP_REGION: {
      // we do not support region struct and don't know how to handle it
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::CLIP_RECT: {
      auto rect = buffer.ReadRect();

      buffer.Validate(rect.has_value());
      BREAK_IF_ERROR(buffer);

      auto packed = buffer.ReadU32();

      auto un_packed_clip_op = packed & 0xF;

      auto offset_to_restore = buffer.ReadInt();
      validate_offset(offset_to_restore);
      BREAK_IF_ERROR(buffer);

      skity_clip_op clip_op = SKITY_CLIP_OP_INTERSECT;

      if (un_packed_clip_op == 0) {
        clip_op = SKITY_CLIP_OP_DIFFERENCE;
      }

      skity_canvas_clip_rect(
          canvas, reinterpret_cast<const skity_rect*>(&rect.value()), clip_op);
    } break;

    case DrawType::CLIP_RRECT: {
      auto rrect = buffer.ReadRRect();

      buffer.Validate(rrect.has_value());
      BREAK_IF_ERROR(buffer);

      auto packed = buffer.ReadU32();

      auto un_packed_clip_op = packed & 0xF;

      auto offset_to_restore = buffer.ReadInt();
      validate_offset(offset_to_restore);
      BREAK_IF_ERROR(buffer);

      skity_clip_op clip_op = SKITY_CLIP_OP_INTERSECT;

      if (un_packed_clip_op == 0) {
        clip_op = SKITY_CLIP_OP_DIFFERENCE;
      }

      core_bridge::PathBuilder builder;
      builder.AddRRect(rrect->rect, rrect->radii, Path::Direction::kCW);
      auto path = builder.Take();

      skity_canvas_clip_path(canvas, path ? wrap_path(*path) : nullptr,
                             clip_op);
    } break;

    case DrawType::CLIP_SHADER_IN_PAINT: {
      const auto* paint = playback_->RequiredPaint(buffer);

      auto clip_op = buffer.ReadInt();

      BREAK_IF_ERROR(buffer);

      // do nothing since we do not support clip shader in paint
    } break;

    case DrawType::RESET_CLIP:
      // do nothing since we do not support this draw op
      break;
    case DrawType::PUSH_CULL:
      break;
    case DrawType::POP_CULL:
      break;

    case DrawType::CONCAT: {
      auto matrix = buffer.ReadMatrix();

      buffer.Validate(matrix.has_value());
      BREAK_IF_ERROR(buffer);

      skity_canvas_concat(
          canvas, reinterpret_cast<const skity_matrix*>(&matrix.value()));
    } break;

    case DrawType::CONCAT44: {
      std::array<float, 16> column_major_matrix{};
      {
        auto ptr = buffer.Skip(16 * sizeof(float));
        buffer.Validate(ptr != nullptr);
        BREAK_IF_ERROR(buffer);

        std::memcpy(column_major_matrix.data(), ptr, 16 * sizeof(float));
      }

      Matrix concat44{
          column_major_matrix[0],  column_major_matrix[1],
          column_major_matrix[2],  column_major_matrix[3],
          column_major_matrix[4],  column_major_matrix[5],
          column_major_matrix[6],  column_major_matrix[7],
          column_major_matrix[8],  column_major_matrix[9],
          column_major_matrix[10], column_major_matrix[11],
          column_major_matrix[12], column_major_matrix[13],
          column_major_matrix[14], column_major_matrix[15],
      };
      skity_canvas_concat(canvas,
                          reinterpret_cast<const skity_matrix*>(&concat44));
    } break;

    case DrawType::DRAW_ANNOTATION: {
      auto rect = buffer.ReadRect();

      buffer.Validate(rect.has_value());
      BREAK_IF_ERROR(buffer);

      std::string key;

      buffer.ReadString(key);

      (void)buffer.ReadByteArrayAsData();

      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_ARC: {
      const auto* paint = playback_->RequiredPaint(buffer);
      auto rect = buffer.ReadRect();

      buffer.Validate(rect.has_value());
      BREAK_IF_ERROR(buffer);

      auto start_angle = buffer.ReadFloat();
      auto sweep_angle = buffer.ReadFloat();
      auto use_center = buffer.ReadBool();

      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_arc(
          canvas, reinterpret_cast<const skity_rect*>(&rect.value()),
          start_angle, sweep_angle, use_center ? 1u : 0u, wrap_paint(paint));
    } break;

    case DrawType::DRAW_ATLAS: {
      const auto* paint = playback_->OptionalPaint(buffer);
      const std::shared_ptr<Image>& image = playback_->GetImage(buffer);

      auto flags = buffer.ReadU32();

      auto count = buffer.ReadU32();

      buffer.Skip(count * 16);  // skip RSXform

      const Rect* tex =
          reinterpret_cast<const Rect*>(buffer.Skip(count * sizeof(Rect)));

      BlendMode blend_mode = BlendMode::kDst;

      const Color* colors = nullptr;

      if (flags & DRAW_ATLAS_HAS_COLORS) {
        colors =
            reinterpret_cast<const Color*>(buffer.Skip(count * sizeof(Color)));
        blend_mode = static_cast<BlendMode>(buffer.ReadU32());

        BREAK_IF_ERROR(buffer);
      }

      const Rect* cull = nullptr;

      if (flags & DRAW_ATLAS_HAS_CULL) {
        cull = reinterpret_cast<const Rect*>(buffer.Skip(sizeof(Rect)));
      }

      BREAK_IF_ERROR(buffer);

      SamplingOptions sampling{};

      if (flags & DRAW_ATLAS_HAS_SAMPLING) {
        sampling = buffer.ReadSamplingOptions();

        BREAK_IF_ERROR(buffer);
      }

      // Draw atlas
    } break;

    case DrawType::DRAW_CLEAR: {
      auto color = buffer.ReadColor();

      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_color(canvas, color, SKITY_BLEND_MODE_SRC);
    } break;

    case DRAW_DATA: {
      auto length = buffer.ReadU32();

      (void)buffer.Skip(length);
    } break;

    case DrawType::DRAW_DRAWABLE:
      // not support drawable
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);

      break;

    case DrawType::DRAW_DRAWABLE_MATRIX:
      // not support drawable matrix
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);

      break;

    case DrawType::DRAW_DRRECT: {
      const auto* paint = playback_->RequiredPaint(buffer);

      auto outer = buffer.ReadRRect();
      auto inner = buffer.ReadRRect();

      buffer.Validate(outer.has_value() && inner.has_value());
      BREAK_IF_ERROR(buffer);

      core_bridge::PathBuilder builder;
      builder.AddRRect(inner->rect, inner->radii, Path::Direction::kCW);
      builder.AddRRect(outer->rect, outer->radii, Path::Direction::kCW);
      builder.SetFillType(Path::PathFillType::kEvenOdd);
      auto path = builder.Take();

      skity_canvas_draw_path(canvas, path ? wrap_path(*path) : nullptr,
                             wrap_paint(paint));
    } break;

    case DrawType::DRAW_EDGEAA_QUAD:
      // not support edge aa quad
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);

      break;

    case DrawType::DRAW_EDGEAA_IMAGE_SET:
    case DrawType::DRAW_EDGEAA_IMAGE_SET2:
      // not support edge aa image set2
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);

      break;

    case DrawType::DRAW_IMAGE: {
      const auto* paint = playback_->OptionalPaint(buffer);

      auto image = playback_->GetImage(buffer);

      auto loc = buffer.ReadPoint();
      auto sampling = buffer.ReadSamplingOptions();

      BREAK_IF_ERROR(buffer);

      skity_sampling_options so = to_sampling(sampling);
      skity_canvas_draw_image_with_sampling(canvas, wrap_image(image), loc.x,
                                            loc.y, &so, wrap_paint(paint));
    } break;

    case DrawType::DRAW_IMAGE_LATTICE: {
      // not support image lattice
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_IMAGE_LATTICE2: {
      // not support image lattice2
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_IMAGE_NINE: {
      // not support image nine
      // fallback to draw image

      const auto* paint = playback_->OptionalPaint(buffer);

      auto image = playback_->GetImage(buffer);

      auto center = buffer.ReadRect();
      auto dst = buffer.ReadRect();

      buffer.Validate(center.has_value() && dst.has_value());
      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_image_rect(
          canvas, wrap_image(image),
          reinterpret_cast<const skity_rect*>(&center.value()),
          reinterpret_cast<const skity_rect*>(&dst.value()), nullptr,
          wrap_paint(paint));
    } break;

    case DrawType::DRAW_IMAGE_RECT: {
      const auto* paint = playback_->OptionalPaint(buffer);

      auto image = playback_->GetImage(buffer);

      Rect storage;

      Rect* src = nullptr;
      if (buffer.ReadBool()) {
        auto tmp = buffer.ReadRect();
        buffer.Validate(tmp.has_value());

        BREAK_IF_ERROR(buffer);

        storage = tmp.value();
        src = &storage;
      }

      auto dst = buffer.ReadRect();

      buffer.Validate(dst.has_value());
      BREAK_IF_ERROR(buffer);

      // skip the constraint
      (void)buffer.ReadU32();

      auto sampling = buffer.ReadSamplingOptions();

      BREAK_IF_ERROR(buffer);

      Rect full = Rect::MakeWH(image->Width(), image->Height());
      const Rect& src_rect = src != nullptr ? *src : full;
      skity_sampling_options so = to_sampling(sampling);
      skity_canvas_draw_image_rect(
          canvas, wrap_image(image),
          reinterpret_cast<const skity_rect*>(&src_rect),
          reinterpret_cast<const skity_rect*>(&dst.value()), &so,
          wrap_paint(paint));
    } break;

    case DrawType::DRAW_IMAGE_RECT2: {
      const auto* paint = playback_->OptionalPaint(buffer);

      auto image = playback_->GetImage(buffer);

      auto src = buffer.ReadRect();
      auto dst = buffer.ReadRect();

      buffer.Validate(src.has_value() && dst.has_value());
      BREAK_IF_ERROR(buffer);

      auto sampling = buffer.ReadSamplingOptions();

      BREAK_IF_ERROR(buffer);

      // skip the constraint
      (void)buffer.ReadU32();

      skity_sampling_options so = to_sampling(sampling);
      skity_canvas_draw_image_rect(
          canvas, wrap_image(image),
          reinterpret_cast<const skity_rect*>(&src.value()),
          reinterpret_cast<const skity_rect*>(&dst.value()), &so,
          wrap_paint(paint));
    } break;

    case DrawType::DRAW_OVAL: {
      const auto* paint = playback_->RequiredPaint(buffer);

      auto oval = buffer.ReadRect();

      buffer.Validate(oval.has_value());
      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_oval(canvas,
                             reinterpret_cast<const skity_rect*>(&oval.value()),
                             wrap_paint(paint));
    } break;

    case DrawType::DRAW_PAINT: {
      const auto* paint = playback_->RequiredPaint(buffer);

      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_paint(canvas, wrap_paint(paint));
    } break;

    case DrawType::DRAW_BEHIND_PAINT: {
      // not support draw behind paint
      // fallback to draw paint
      const auto* paint = playback_->RequiredPaint(buffer);

      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_paint(canvas, wrap_paint(paint));
    } break;

    case DrawType::DRAW_PATCH: {
      // not support draw patch
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_PATH: {
      const auto* paint = playback_->RequiredPaint(buffer);

      const auto& path = playback_->GetPath(buffer);

      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_path(canvas, wrap_path(path), wrap_paint(paint));
    } break;

    case DrawType::DRAW_PICTURE: {
      // not support draw picture
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_PICTURE_MATRIX_PAINT: {
      // not support draw picture matrix paint
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_POINTS: {
      const auto* paint = playback_->RequiredPaint(buffer);

      buffer.ReadU32();  // skip mode

      size_t count = buffer.ReadU32();

      buffer.Skip(count * 2 * sizeof(float));

      BREAK_IF_ERROR(buffer);

      // skip draw
    } break;

    case DrawType::DRAW_RECT: {
      const auto* paint = playback_->RequiredPaint(buffer);

      auto rect = buffer.ReadRect();

      buffer.Validate(rect.has_value());
      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_rect(canvas,
                             reinterpret_cast<const skity_rect*>(&rect.value()),
                             wrap_paint(paint));
    } break;

    case DrawType::DRAW_REGION: {
      // not support draw region
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_RRECT: {
      const auto* paint = playback_->RequiredPaint(buffer);

      auto rrect = buffer.ReadRRect();

      buffer.Validate(rrect.has_value());
      BREAK_IF_ERROR(buffer);

      // Same dispatch Canvas::DrawRRect performs: convert to a path and
      // draw that.
      core_bridge::PathBuilder builder;
      builder.AddRRect(rrect->rect, rrect->radii, Path::Direction::kCW);
      auto rrect_path = builder.Take();
      skity_canvas_draw_path(canvas,
                             rrect_path ? wrap_path(*rrect_path) : nullptr,
                             wrap_paint(paint));
    } break;

    case DrawType::DRAW_SHADOW_REC: {
      const auto& path = playback_->GetPath(buffer);

      Vec3 z_panel;
      buffer.ReadPad32(&z_panel, sizeof(Vec3));

      Vec3 light_post;
      buffer.ReadPad32(&light_post, sizeof(Vec3));  // light pos

      auto light_radius = buffer.ReadFloat();   // light radius
      auto ambient_color = buffer.ReadColor();  // ambient color
      auto spot_color = buffer.ReadColor();     // spot color
      auto flags = buffer.ReadU32();            // flags

      BREAK_IF_ERROR(buffer);

      if (ambient_color > 0) {
        auto dev_space_outset = ambient_blur_radius(z_panel.z);
        auto one_over_a = ambient_recip_alpha(z_panel.z);

        auto blur_radius = 0.5f * dev_space_outset * one_over_a;
        auto stroke_width = 0.5f * (dev_space_outset - blur_radius);

        Matrix ctm{};
        skity_canvas_get_total_matrix(canvas,
                                      reinterpret_cast<skity_matrix*>(&ctm));
        auto scale = ctm.GetScaleY();

        auto paint = core_bridge::MakePaint();
        if (paint) {
          core_bridge::SetPaintColor(paint, ambient_color);
          // prevent stroke width being scaled
          core_bridge::SetPaintStrokeWidth(paint, stroke_width / scale);
          core_bridge::SetPaintStyle(paint, Paint::kStrokeAndFill_Style);
          core_bridge::SetPaintMaskFilter(
              paint,
              core_bridge::MakeMaskFilterBlur(BlurStyle::kNormal, blur_radius));
        }

        skity_canvas_draw_path(canvas, wrap_path(path),
                               paint ? wrap_paint(paint.get()) : nullptr);
      }

      if (spot_color > 0) {
        bool diractional = flags & 0x04;  // kDirectionalLight_ShadowFlag

        Matrix m{};
        float radius;

        Matrix ctm{};
        skity_canvas_get_total_matrix(canvas,
                                      reinterpret_cast<skity_matrix*>(&ctm));
        if (!get_spot_shadow_transform(light_post, light_radius, ctm, z_panel,
                                       core_bridge::GetPathBounds(&path),
                                       diractional, m, radius)) {
          break;
        }

        auto paint = core_bridge::MakePaint();
        if (paint) {
          core_bridge::SetPaintColor(paint, spot_color);
          core_bridge::SetPaintMaskFilter(
              paint,
              core_bridge::MakeMaskFilterBlur(BlurStyle::kNormal, radius));
        }

        skity_canvas_save(canvas);
        skity_canvas_concat(canvas, reinterpret_cast<const skity_matrix*>(&m));
        skity_canvas_draw_path(canvas, wrap_path(path),
                               paint ? wrap_paint(paint.get()) : nullptr);

        skity_canvas_restore(canvas);
      }
    } break;

    case DrawType::DRAW_TEXT_BLOB: {
      const auto* paint = playback_->RequiredPaint(buffer);

      const auto& blob = playback_->GetTextBlob(buffer);

      auto x = buffer.ReadFloat();
      auto y = buffer.ReadFloat();

      BREAK_IF_ERROR(buffer);

      skity_canvas_draw_text_blob(canvas, wrap_blob(blob), x, y,
                                  wrap_paint(paint));
    } break;

    case DrawType::DRAW_SLUG: {
      // not support draw slug
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::DRAW_VERTICES_OBJECT: {
      // not support draw vertices object
      buffer.Validate(false);
      BREAK_IF_ERROR(buffer);
    } break;

    case DrawType::RESTORE:
      skity_canvas_restore(canvas);
      break;

    case DrawType::ROTATE: {
      auto deg = buffer.ReadFloat();

      skity_canvas_rotate(canvas, deg);
    } break;

    case DrawType::SAVE:
      skity_canvas_save(canvas);
      break;

    case DrawType::SAVE_BEHIND: {
      // not support save behind

      auto flags = buffer.ReadU32();

      if (flags & SAVEBEHIND_HAS_SUBSET) {
        (void)buffer.ReadRect();
      }

      BREAK_IF_ERROR(buffer);
      skity_canvas_save(canvas);
    } break;

    case DrawType::SAVE_LAYER_SAVELAYERREC: {
      auto flat_flags = buffer.ReadU32();

      std::optional<Rect> bounds;
      const Paint* paint = nullptr;

      std::shared_ptr<ImageFilter> back_drop;
      uint32_t layer_flag = 0;

      if (flat_flags & SAVELAYERREC_HAS_BOUNDS) {
        bounds = buffer.ReadRect();

        BREAK_IF_ERROR(buffer);
      }

      if (flat_flags & SAVELAYERREC_HAS_PAINT) {
        paint = playback_->RequiredPaint(buffer);
      }

      if (flat_flags & SAVELAYERREC_HAS_BACKDROP) {
        const auto* backdrop_paint = playback_->RequiredPaint(buffer);

        back_drop = backdrop_paint != nullptr ? backdrop_paint->GetImageFilter()
                                              : nullptr;
      }

      if (flat_flags & SAVELAYERREC_HAS_FLAGS) {
        layer_flag = buffer.ReadU32();
      }

      if (flat_flags & SAVELAYERREC_HAS_CLIPMASK_OBSOLETE) {
        (void)playback_->GetImage(buffer);
      }

      if (flat_flags & SAVELAYERREC_HAS_CLIPMATRIX_OBSOLETE) {
        (void)buffer.ReadMatrix();

        BREAK_IF_ERROR(buffer);
      }

      if (!buffer.IsVersionLT(Version::kBackdropScaleFactor) &&
          (flat_flags & SAVELAYERREC_HAS_BACKDROP_SCALE)) {
        (void)buffer.ReadFloat();  // do not support drop shadow scale
      }

      if (!buffer.IsVersionLT(Version::kMultipleFiltersOnSaveLayer) &&
          (flat_flags & SAVELAYERREC_HAS_MULTIPLE_FILTERS)) {
        auto count = buffer.ReadU32();
        buffer.Validate(count > 0 && count <= 16);  // kMaxFiltersPerLayer
        BREAK_IF_ERROR(buffer);

        for (uint32_t i = 0; i < count; i++) {
          (void)playback_->RequiredPaint(buffer);
        }
      }

      if (!buffer.IsVersionLT(Version::kSaveLayerBackdropTileMode) &&
          (flat_flags & SAVELAYERREC_HAS_BACKDROP_TILEMODE)) {
        (void)buffer.ReadU32();  // skip the tile mode
      }

      BREAK_IF_ERROR(buffer);

      if (!bounds.has_value()) {
        Rect clip_bounds{};
        skity_canvas_get_global_clip_bounds(
            canvas, reinterpret_cast<skity_rect*>(&clip_bounds));
        bounds = clip_bounds;
      }

      skity_canvas_save_layer(
          canvas, reinterpret_cast<const skity_rect*>(&bounds.value()),
          wrap_paint(paint));
    } break;

    case DrawType::SCALE: {
      auto sx = buffer.ReadFloat();
      auto sy = buffer.ReadFloat();

      skity_canvas_scale(canvas, sx, sy);
    } break;

    case DrawType::SET_M44: {
      std::array<float, 16> matrix_data{};

      {
        auto ptr = buffer.Skip(matrix_data.size() * sizeof(float));

        BREAK_IF_ERROR(buffer);

        std::memcpy(matrix_data.data(), ptr,
                    matrix_data.size() * sizeof(float));
      }

      Matrix m{
          matrix_data[0],  matrix_data[1],  matrix_data[2],  matrix_data[3],
          matrix_data[4],  matrix_data[5],  matrix_data[6],  matrix_data[7],
          matrix_data[8],  matrix_data[9],  matrix_data[10], matrix_data[11],
          matrix_data[12], matrix_data[13], matrix_data[14], matrix_data[15],
      };

      skity_canvas_set_matrix(canvas,
                              reinterpret_cast<const skity_matrix*>(&m));
    } break;

    case DrawType::SET_MATRIX: {
      auto m = buffer.ReadMatrix();

      buffer.Validate(m.has_value());

      BREAK_IF_ERROR(buffer);

      skity_canvas_set_matrix(
          canvas, reinterpret_cast<const skity_matrix*>(&m.value()));
    } break;

    case DrawType::SKEW: {
      auto sx = buffer.ReadFloat();
      auto sy = buffer.ReadFloat();

      skity_canvas_skew(canvas, sx, sy);
    } break;

    case DrawType::TRANSLATE: {
      auto dx = buffer.ReadFloat();
      auto dy = buffer.ReadFloat();

      skity_canvas_translate(canvas, dx, dy);
    } break;

    default:
      buffer.Validate(false);
      break;
  }

#undef BREAK_IF_ERROR
}  // NOLINT(readability/fn_size)

}  // namespace skity
