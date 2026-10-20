// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/record/record_playback.hpp"

#include <cstdlib>
#include <iostream>

#include "src/io/core_bridge.hpp"
#include "src/io/flat/font_desc_flat.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/read/read_typeface.hpp"
#include "src/picture.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

static constexpr int32_t kUInt32Size = 4;

RecordPlayback* self_of(void* userdata) {
  return static_cast<RecordPlayback*>(userdata);
}

// Delegate-canvas trampolines: unwrap the borrowed C handles back into the
// legacy object references the On* members record from. Every handle passed
// by the core delegate is valid for the duration of the callback only.
std::shared_ptr<Paint> paint_of(skity_paint handle) {
  return skity::capi::get_impl<skity_paint_s, Paint>(handle,
                                                     SKITY_OBJECT_TYPE_PAINT);
}

Path path_of(skity_path handle) {
  auto sp =
      skity::capi::get_impl<skity_path_s, Path>(handle, SKITY_OBJECT_TYPE_PATH);
  return sp ? *sp : Path{};
}

std::shared_ptr<TextBlob> blob_of(skity_text_blob handle) {
  return skity::capi::get_impl<skity_text_blob_s, TextBlob>(
      handle, SKITY_OBJECT_TYPE_TEXT_BLOB);
}

std::shared_ptr<Image> image_of(skity_image handle) {
  return skity::capi::get_impl<skity_image_s, Image>(handle,
                                                     SKITY_OBJECT_TYPE_IMAGE);
}

void tp_save(void* ud) { self_of(ud)->OnSave(); }

void tp_restore(void* ud) { self_of(ud)->OnRestore(); }

void tp_save_layer(void* ud, const skity_rect* bounds, skity_paint paint) {
  self_of(ud)->OnSaveLayer(*reinterpret_cast<const Rect*>(bounds),
                           paint_of(paint));
}

void tp_translate(void* ud, float dx, float dy) {
  self_of(ud)->OnTranslate(dx, dy);
}

void tp_scale(void* ud, float sx, float sy) { self_of(ud)->OnScale(sx, sy); }

void tp_rotate(void* ud, float degrees) { self_of(ud)->OnRotate(degrees); }

void tp_rotate_point(void* ud, float degrees, float px, float py) {
  self_of(ud)->OnRotate(degrees, px, py);
}

void tp_skew(void* ud, float sx, float sy) { self_of(ud)->OnSkew(sx, sy); }

void tp_concat(void* ud, const skity_matrix* matrix) {
  self_of(ud)->OnConcat(*reinterpret_cast<const Matrix*>(matrix));
}

void tp_set_matrix(void* ud, const skity_matrix* matrix) {
  self_of(ud)->OnSetMatrix(*reinterpret_cast<const Matrix*>(matrix));
}

void tp_reset_matrix(void* ud) { self_of(ud)->OnResetMatrix(); }

void tp_clip_rect(void* ud, const skity_rect* rect, skity_clip_op op) {
  self_of(ud)->OnClipRect(*reinterpret_cast<const Rect*>(rect), op);
}

void tp_clip_path(void* ud, skity_path path, skity_clip_op op) {
  self_of(ud)->OnClipPath(path_of(path), op);
}

void tp_draw_paint(void* ud, skity_paint paint) {
  self_of(ud)->OnDrawPaint(paint_of(paint));
}

void tp_draw_rect(void* ud, const skity_rect* rect, skity_paint paint) {
  self_of(ud)->OnDrawRect(*reinterpret_cast<const Rect*>(rect),
                          paint_of(paint));
}

void tp_draw_path(void* ud, skity_path path, skity_paint paint) {
  self_of(ud)->OnDrawPath(path_of(path), paint_of(paint));
}

void tp_draw_blob(void* ud, skity_text_blob blob, float x, float y,
                  skity_paint paint) {
  self_of(ud)->OnDrawBlob(blob_of(blob), x, y, paint_of(paint));
}

void tp_draw_image_rect(void* ud, skity_image image, const skity_rect* src,
                        const skity_rect* dst,
                        const skity_sampling_options* sampling,
                        skity_paint paint) {
  SamplingOptions so{};
  if (sampling != nullptr) {
    so.filter = static_cast<FilterMode>(sampling->filter);
    so.mipmap = static_cast<MipmapMode>(sampling->mipmap);
    so.cubic.B = sampling->cubic_b;
    so.cubic.C = sampling->cubic_c;
  }
  self_of(ud)->OnDrawImageRect(
      image_of(image), *reinterpret_cast<const Rect*>(src),
      *reinterpret_cast<const Rect*>(dst), so, paint_of(paint));
}

void tp_draw_glyphs(void* ud, uint32_t count, const uint16_t* glyphs,
                    const float* positions_x, const float* positions_y,
                    skity_font font, skity_paint paint) {
  self_of(ud)->OnDrawGlyphs(
      count, reinterpret_cast<const GlyphID*>(glyphs), positions_x, positions_y,
      skity::capi::get_impl<skity_font_s, Font>(font, SKITY_OBJECT_TYPE_FONT),
      paint_of(paint));
}

skity_canvas_delegate_callbacks delegate_callbacks() {
  skity_canvas_delegate_callbacks cb{};
  cb.on_save = &tp_save;
  cb.on_restore = &tp_restore;
  cb.on_save_layer = &tp_save_layer;
  cb.on_translate = &tp_translate;
  cb.on_scale = &tp_scale;
  cb.on_rotate = &tp_rotate;
  cb.on_rotate_point = &tp_rotate_point;
  cb.on_skew = &tp_skew;
  cb.on_concat = &tp_concat;
  cb.on_set_matrix = &tp_set_matrix;
  cb.on_reset_matrix = &tp_reset_matrix;
  cb.on_clip_rect = &tp_clip_rect;
  cb.on_clip_path = &tp_clip_path;
  cb.on_draw_paint = &tp_draw_paint;
  cb.on_draw_rect = &tp_draw_rect;
  cb.on_draw_path = &tp_draw_path;
  cb.on_draw_blob = &tp_draw_blob;
  cb.on_draw_image_rect = &tp_draw_image_rect;
  cb.on_draw_glyphs = &tp_draw_glyphs;
  return cb;
}

void write_tag_size(WriteStream& stream, uint32_t tag, size_t size) {
  stream.WriteU32(tag);
  stream.WriteU32(static_cast<uint32_t>(size));
}

void write_tag_size(BinaryWriteBuffer& buffer, uint32_t tag, size_t size) {
  buffer.WriteUint32(tag);
  buffer.WriteUint32(static_cast<uint32_t>(size));
}

size_t compute_chunk_size(const FactorySet& factory_set) {
  size_t size = 4;

  for (size_t i = 0; i < factory_set.factories.size(); i++) {
    const auto& factory = factory_set.factories[i];

    if (factory.empty()) {
      size += WriteStream::PackedUintSize(0);
    } else {
      auto len = factory.size();
      size += WriteStream::PackedUintSize(len);
      size += len;
    }
  }

  return size;
}

template <typename T>
int32_t find_or_append(std::vector<T>& array, const T& value) {
  for (size_t i = 0; i < array.size(); i++) {
    if (array[i] == value) {
      return static_cast<int32_t>(i);
    }
  }

  array.emplace_back(value);

  return static_cast<int32_t>(array.size() - 1);
}

template <typename T, typename Proc>
bool parse_array_from_buffer(ReadBuffer& buffer, uint32_t count,
                             std::vector<std::shared_ptr<T>>& array,
                             Proc&& proc) {
  if (!buffer.IsValid()) {
    return false;
  }

  if (count == 0) {
    return true;
  }

  for (uint32_t i = 0; i < count; i++) {
    auto value = proc(buffer);

    if (!buffer.Validate(value != nullptr)) {
      array.clear();

      return false;
    }

    array.emplace_back(value);
  }

  return true;
}

template <typename T>
void skip_array_from_buffer(ReadBuffer& buffer, uint32_t count) {
  for (uint32_t i = 0; i < count; i++) {
    SkipFromMemory<T>(buffer);

    if (!buffer.IsValid()) {
      break;
    }
  }
}

}  // namespace

RecordPlayback::RecordPlayback(uint32_t width, uint32_t height)
    : width_(width), height_(height) {}

RecordPlayback::RecordPlayback(uint32_t width, uint32_t height,
                               int32_t target_version)
    : width_(width), height_(height), target_version_(target_version) {}

RecordPlayback::~RecordPlayback() {
  if (delegate_ != nullptr) {
    skity_canvas_destroy(delegate_);
    delegate_ = nullptr;
  }
}

// The save / restore depth bookkeeping the Canvas base used to perform lives
// inside the core delegate now; the serialized bytes come solely from the
// On* recorders, so driving them directly keeps the format identical.
void RecordPlayback::BeginRecording() { OnSave(); }

void RecordPlayback::EndRecording() { OnRestore(); }

skity_canvas RecordPlayback::GetDelegateCanvas() {
  if (delegate_ == nullptr) {
    auto cb = delegate_callbacks();
    delegate_ = skity_canvas_create_delegate(&cb, this);
  }
  return delegate_;
}

void RecordPlayback::Serialize(WriteStream& stream, const SerialProc* proc,
                               TypefaceSet* top_typeface_set) {
  auto op_data = writer32_.MakeSnapshot();

  write_tag_size(stream, SK_PICT_READER_TAG, op_data->Size());
  stream.Write(op_data->Bytes(), op_data->Size());

  TypefaceSet local_typeface_set;

  TypefaceSet* typeface_set =
      top_typeface_set ? top_typeface_set : &local_typeface_set;
  FactorySet factory_set;

  BinaryWriteBuffer buffer;

  buffer.SetTypefaceSet(typeface_set);
  buffer.SetFactorySet(&factory_set);
  buffer.SetSerialProc(proc);

  FlattenToBuffer(buffer);

  // will support recursive flatten sub picture

  // write factory set section
  WriteFactories(stream, factory_set);

  // Write typeface set section
  WriteTypefaces(stream, *typeface_set);

  // write the data section
  write_tag_size(stream, SK_PICT_BUFFER_SIZE_TAG, buffer.BytesWritten());
  buffer.WriteToStream(stream);

  // do not support recursive flatten sub picture

  stream.WriteU32(SK_PICT_EOF_TAG);
}

std::unique_ptr<RecordPlayback> RecordPlayback::CreateFromStream(
    const Rect& cull_rect, int32_t target_version, ReadStream& stream,
    TypefaceSet* typeface_set, int32_t recursion_limit) {
  auto playback = std::make_unique<RecordPlayback>(
      cull_rect.Width(), cull_rect.Height(), target_version);

  if (typeface_set == nullptr) {
    typeface_set = &playback->playback_typeface_set_;
  }

  if (!playback->ParseStream(stream, typeface_set, recursion_limit)) {
    return {};
  }

  return playback;
}

void RecordPlayback::OnClipRect(Rect const& rect, skity_clip_op op) {
  size_t size = 1 * kUInt32Size + sizeof(rect) + 1 * kUInt32Size;

  if (!restore_offset_stack_.empty()) {
    size += kUInt32Size;
  }

  size_t init_offset = AddDraw(DrawType::CLIP_RECT, size);

  AddRect(rect);
  // ignore aa logical
  AddInt(static_cast<int32_t>(op));

  RecordRestoreOffsetPlaceHolder();

  Validate(init_offset, size);
}

void RecordPlayback::OnClipPath(Path const& path, skity_clip_op op) {
  // add path to path list
  auto path_id = AddPath(path);

  // record clip path
  size_t size = 3 * kUInt32Size;

  if (!restore_offset_stack_.empty()) {
    size += kUInt32Size;
  }

  auto offset = AddDraw(DrawType::CLIP_PATH, size);

  AddInt(path_id);
  AddInt(static_cast<int32_t>(op));

  RecordRestoreOffsetPlaceHolder();

  Validate(offset, size);
}

void RecordPlayback::OnDrawRect(const Rect& rect,
                                std::shared_ptr<Paint> paint) {
  // op + paint index + rect
  size_t size = 2 * kUInt32Size + sizeof(rect);

  auto offset = AddDraw(DrawType::DRAW_RECT, size);

  AddPaint(paint);
  AddRect(rect);

  Validate(offset, size);
}

void RecordPlayback::OnDrawPath(const Path& path,
                                std::shared_ptr<Paint> paint) {
  // op + paint index + path index
  size_t size = 3 * kUInt32Size;

  auto offset = AddDraw(DrawType::DRAW_PATH, size);

  AddPaint(paint);

  auto path_id = AddPath(path);
  AddInt(path_id);

  Validate(offset, size);
}

void RecordPlayback::OnSaveLayer(const Rect& bounds,
                                 std::shared_ptr<Paint> paint) {
  restore_offset_stack_.emplace_back(
      -static_cast<int32_t>(writer32_.BytesWritten()));

  // record save layer
  // Note:
  //        skity always have bounds and paint.
  //        And don't have other savelayer flags

  // op + flatflags
  size_t size = 2 * kUInt32Size;

  // always have bounds and paint
  uint32_t flat_flags = SAVELAYERREC_HAS_BOUNDS | SAVELAYERREC_HAS_PAINT;
  size += sizeof(Rect) + sizeof(uint32_t);  // bounds + paint index

  auto offset = AddDraw(SAVE_LAYER_SAVELAYERREC, size);

  AddInt(flat_flags);

  AddRect(bounds);
  AddPaint(paint);

  Validate(offset, size);
}

void RecordPlayback::OnDrawBlob(std::shared_ptr<TextBlob> blob, float x,
                                float y, std::shared_ptr<Paint> paint) {
  // op + paint index + blob index + x/y
  size_t size = 3 * kUInt32Size + sizeof(float) * 2;

  auto offset = AddDraw(DrawType::DRAW_TEXT_BLOB, size);

  // Retain the blob by re-creating it through the C ABI (multi-run form),
  // keeping TextRun construction on the core side.
  std::vector<core_bridge::TextRunData> runs;
  core_bridge::WithTextBlobHandle(blob.get(), [&](skity_text_blob handle) {
    int32_t run_count =
        handle != nullptr ? skity_text_blob_get_run_count(handle) : 0;
    for (int32_t i = 0; i < run_count; i++) {
      core_bridge::TextRunData data;
      skity_font font_handle = skity_text_blob_get_run_font(handle, i);
      data.font = skity::capi::get_impl<skity_font_s, Font>(
          font_handle, SKITY_OBJECT_TYPE_FONT);
      skity_font_destroy(font_handle);

      int32_t glyph_count = skity_text_blob_get_run_glyph_count(handle, i);
      data.glyphs.resize(glyph_count > 0 ? glyph_count : 0);
      if (!data.glyphs.empty()) {
        skity_text_blob_get_run_glyphs(handle, i, data.glyphs.data(),
                                       glyph_count);
      }
      int32_t px = skity_text_blob_get_run_pos_x(handle, i, nullptr, 0);
      int32_t py = skity_text_blob_get_run_pos_y(handle, i, nullptr, 0);
      data.pos_x.resize(px > 0 ? px : 0);
      data.pos_y.resize(py > 0 ? py : 0);
      if (!data.pos_x.empty()) {
        skity_text_blob_get_run_pos_x(handle, i, data.pos_x.data(), px);
      }
      if (!data.pos_y.empty()) {
        skity_text_blob_get_run_pos_y(handle, i, data.pos_y.data(), py);
      }
      runs.emplace_back(std::move(data));
    }
    return 0;
  });
  auto retained = core_bridge::MakeTextBlobFromRuns(runs);

  AddPaint(std::move(paint));
  AddTextBlob(retained);
  AddFloat(x);
  AddFloat(y);

  Validate(offset, size);
}

void RecordPlayback::OnDrawImageRect(std::shared_ptr<Image> image,
                                     const Rect& src, const Rect& dst,
                                     const SamplingOptions& sampling,
                                     std::shared_ptr<Paint> paint) {
  size_t size = 3 * kUInt32Size + 2 * sizeof(Rect) +
                (sizeof(uint32_t)        // maxAniso
                 + 3 * sizeof(uint32_t)  // bool32 + [2 floats | 2 ints]
                 ) +                     // NOLINT
                kUInt32Size              // kFast_SrcRectConstraint
      ;                                  // NOLINT

  auto offset = AddDraw(DRAW_IMAGE_RECT2, size);

  AddPaintPtr(paint);
  AddImage(image);
  AddRect(src);
  AddRect(dst);

  writer32_.WriteSampling(sampling);
  writer32_.WriteInt32(1);  // kFast_SrcRectConstraint

  Validate(offset, size);
}

void RecordPlayback::OnDrawGlyphs(uint32_t count, const GlyphID glyphs[],
                                  const float position_x[],
                                  const float position_y[],
                                  std::shared_ptr<Font> font,
                                  std::shared_ptr<Paint> paint) {
  // not support
}

void RecordPlayback::OnDrawPaint(std::shared_ptr<Paint> paint) {
  // op + paint index
  size_t size = 2 * kUInt32Size;

  auto offset = AddDraw(DrawType::DRAW_PAINT, size);

  AddPaint(paint);

  Validate(offset, size);
}

void RecordPlayback::OnSave() {
  // record offset
  restore_offset_stack_.push_back(
      -static_cast<int32_t>(writer32_.BytesWritten()));

  // record draw type
  size_t size = sizeof(kUInt32Size);
  size_t offset = AddDraw(DrawType::SAVE, size);

  Validate(offset, size);
}

void RecordPlayback::OnRestore() {
  if (restore_offset_stack_.empty()) {
    return;
  }

  // record draw type
  FillRestoreOffsetPlaceholder(static_cast<uint32_t>(writer32_.BytesWritten()));

  size_t size = 1 * kUInt32Size;
  size_t offset = AddDraw(DrawType::RESTORE, size);

  Validate(offset, size);

  // pop restore offset
  restore_offset_stack_.pop_back();
}

void RecordPlayback::OnTranslate(float dx, float dy) {
  // op + dx + dy
  size_t size = 1 * kUInt32Size + sizeof(float) * 2;

  auto offset = AddDraw(DrawType::TRANSLATE, size);

  AddFloat(dx);
  AddFloat(dy);

  Validate(offset, size);
}

void RecordPlayback::OnScale(float sx, float sy) {
  // op + sx + sy
  size_t size = 1 * kUInt32Size + sizeof(float) * 2;

  auto offset = AddDraw(DrawType::SCALE, size);

  AddFloat(sx);
  AddFloat(sy);

  Validate(offset, size);
}

void RecordPlayback::OnRotate(float degrees) {
  auto m = core_bridge::RotateDegMatrix(degrees);

  OnConcat(m);
}

void RecordPlayback::OnRotate(float degree, float px, float py) {
  auto m = core_bridge::RotateDegMatrix(degree, Vec2{px, py});

  OnConcat(m);
}

void RecordPlayback::OnSkew(float sx, float sy) {
  auto m = Matrix::Skew(sx, sy);

  OnConcat(m);
}

void RecordPlayback::OnConcat(const Matrix& matrix) {
  Validate(writer32_.BytesWritten(), 0);

  // op + matrix
  size_t size = 1 * kUInt32Size + 9 * sizeof(float);  // uint32 + 9 float

  auto offset = AddDraw(DrawType::CONCAT, size);

  AddMatrix(matrix);

  Validate(offset, size);
}

void RecordPlayback::OnSetMatrix(const Matrix& matrix) {
  Validate(writer32_.BytesWritten(), 0);

  size_t size = 1 * kUInt32Size + 16 * sizeof(float);  // uint32 + 16 float

  auto offset = AddDraw(DrawType::SET_M44, size);

  writer32_.Write(&matrix, sizeof(matrix));

  Validate(offset, size);
}

void RecordPlayback::OnResetMatrix() { OnSetMatrix(Matrix{}); }

bool RecordPlayback::ParseStream(ReadStream& stream, TypefaceSet* typeface_set,
                                 int32_t recursion_limit) {
  while (true) {
    uint32_t tag;
    if (!stream.ReadU32(&tag)) {
      return false;
    }

    if (tag == SK_PICT_EOF_TAG) {
      break;
    }

    uint32_t size;
    if (!stream.ReadU32(&size)) {
      return false;
    }

    if (!ParseStreamTag(stream, tag, size, typeface_set, recursion_limit)) {
      return false;
    }
  }

  return true;
}

void RecordPlayback::FlattenToBuffer(BinaryWriteBuffer& buffer) {
  auto num_paints = static_cast<int32_t>(paints_.size());
  if (num_paints > 0) {
    // write paint section
    write_tag_size(buffer, SK_PICT_PAINT_BUFFER_TAG, num_paints);

    for (const auto& paint : paints_) {
      buffer.WritePaint(*paint);
    }
  }

  auto num_paths = static_cast<int32_t>(paths_.size());
  if (num_paths > 0) {
    // write path section
    write_tag_size(buffer, SK_PICT_PATH_BUFFER_TAG, num_paths);

    buffer.WriteInt32(num_paths);

    for (const auto& path : paths_) {
      buffer.WritePath(path);
    }
  }

  auto num_text_blobs = static_cast<int32_t>(text_blobs_.size());
  if (num_text_blobs > 0) {
    write_tag_size(buffer, SK_PICT_TEXTBLOB_BUFFER_TAG, num_text_blobs);

    for (const auto& text_blob : text_blobs_) {
      buffer.WriteTextBlob(*text_blob);
    }
  }

  auto image_count = static_cast<int32_t>(images_.size());
  if (image_count > 0) {
    write_tag_size(buffer, SK_PICT_IMAGE_BUFFER_TAG, image_count);

    for (const auto& image : images_) {
      buffer.WriteImage(image.get());
    }
  }
}

void RecordPlayback::WriteFactories(WriteStream& stream,
                                    const FactorySet& factory_set) {
  int32_t count = static_cast<int32_t>(factory_set.factories.size());

  auto size = compute_chunk_size(factory_set);

  write_tag_size(stream, SK_PICT_FACTORY_TAG, size);

  size_t start = stream.BytesWritten();

  stream.WriteU32(count);

  for (int32_t i = 0; i < count; i++) {
    const auto& name = factory_set.factories[i];

    if (name.empty()) {
      stream.WritePackedUint(0);
    } else {
      auto len = name.size();
      stream.WritePackedUint(len);
      stream.Write(name.c_str(), len);
    }
  }

  if (size != (stream.BytesWritten() - start)) {
    std::cerr << "WriteFactories size not match" << std::endl;
    std::cerr << "expect size: " << size << std::endl;
    std::cerr << "actual size: " << (stream.BytesWritten() - start)
              << std::endl;
  }
}

void RecordPlayback::WriteTypefaces(WriteStream& stream,
                                    const TypefaceSet& typeface_set) {
  auto count = static_cast<int32_t>(typeface_set.typefaces.size());

  write_tag_size(stream, SK_PICT_TYPEFACE_TAG, count);

  for (int32_t i = 0; i < count; i++) {
    const auto& typeface = typeface_set.typefaces[i];

    // Descriptor fields read through the C ABI; the legacy FontDescriptor
    // is never materialized on this side.
    FontDescriptorFields fields;

    {
      std::shared_ptr<Typeface> view(typeface.get(), [](Typeface*) {});
      skity_typeface handle = skity::capi::alloc_handle<skity_typeface_s>(
          SKITY_OBJECT_TYPE_TYPEFACE, 0, std::move(view));

      skity_font_descriptor fd{};
      int32_t family_len =
          skity_typeface_get_font_descriptor(handle, &fd, nullptr, 0);
      fields.family_name.resize(family_len > 0 ? family_len - 1 : 0);
      skity_typeface_get_font_descriptor(handle, &fd, fields.family_name.data(),
                                         family_len);

      int32_t full_len = skity_typeface_get_full_name(handle, nullptr, 0);
      fields.full_name.resize(full_len > 0 ? full_len - 1 : 0);
      skity_typeface_get_full_name(handle, fields.full_name.data(), full_len);

      int32_t post_len =
          skity_typeface_get_post_script_name(handle, nullptr, 0);
      fields.post_script_name.resize(post_len > 0 ? post_len - 1 : 0);
      skity_typeface_get_post_script_name(
          handle, fields.post_script_name.data(), post_len);

      int32_t coord_count =
          skity_typeface_get_variation_position(handle, nullptr, 0);
      std::vector<skity_variation_coordinate> coords(
          coord_count > 0 ? coord_count : 0);
      if (!coords.empty()) {
        skity_typeface_get_variation_position(
            handle, coords.data(), static_cast<int32_t>(coords.size()));
      }
      for (const auto& coord : coords) {
        fields.variations.emplace_back(coord.axis, coord.value);
      }

      fields.collection_index = fd.collection_index;
      fields.factory_id = fd.factory_id;
      // The style components come back through the same packing the legacy
      // FontStyle word uses.
      fields.style_packed = (static_cast<uint32_t>(fd.style.weight) << 16) |
                            (static_cast<uint32_t>(fd.style.width) << 8) |
                            static_cast<uint32_t>(fd.style.slant);

      skity_typeface_destroy(handle);
    }

    SerializeFontDescriptorFields(stream, fields);

    if (typeface) {
      auto data = typeface->GetData();
      stream.WritePackedUint(data->Size());
      stream.Write(data->RawData(), data->Size());
    } else {
      stream.WritePackedUint(0);
    }
  }
}

size_t RecordPlayback::RecordRestoreOffsetPlaceHolder() {
  if (restore_offset_stack_.empty()) {
    return -1;
  }

  int32_t prev_offset = restore_offset_stack_.back();

  size_t offset = writer32_.BytesWritten();

  AddInt(prev_offset);

  restore_offset_stack_.back() = static_cast<uint32_t>(offset);

  return offset;
}

void RecordPlayback::FillRestoreOffsetPlaceholder(uint32_t restore_offset) {
  auto offset = restore_offset_stack_.back();

  while (offset > 0) {
    auto peek = writer32_.ReadAt<uint32_t>(offset);
    writer32_.OverwriteAt(offset, restore_offset);

    offset = peek;
  }
}

size_t RecordPlayback::AddDraw(DrawType type, size_t& size) {
  size_t offset = writer32_.BytesWritten();

  if (size == 0) {
    // size should be greater than 0
    return 0;
  }

  if (0 != (size & ~MASK_24) || size == MASK_24) {
    writer32_.WriteInt32(PACK_8_24(type, MASK_24));
    size += 1;
    writer32_.WriteInt32(static_cast<uint32_t>(size));
  } else {
    writer32_.WriteInt32(PACK_8_24(type, static_cast<uint32_t>(size)));
  }

  return offset;
}

void RecordPlayback::AddInt(int32_t value) { writer32_.WriteInt32(value); }
void RecordPlayback::AddFloat(float value) { writer32_.WriteFloat(value); }
void RecordPlayback::AddMatrix(const Matrix& matrix) {
  writer32_.WriteMatrix(matrix);
}

void RecordPlayback::AddRect(const Rect& rect) { writer32_.WriteRect(rect); }

void RecordPlayback::AddPaint(std::shared_ptr<Paint> paint) {
  AddPaintPtr(std::move(paint));
}

void RecordPlayback::AddPaintPtr(std::shared_ptr<Paint> paint) {
  if (paint) {
    paints_.push_back(std::move(paint));
    AddInt(static_cast<int32_t>(paints_.size()));
  } else {
    AddInt(0);
  }
}

int32_t RecordPlayback::AddPath(const Path& path) {
  // we do not try to find existing path

  int32_t n = paths_.size() + 1;

  paths_.emplace_back(path);
  return n;
}

void RecordPlayback::AddImage(const std::shared_ptr<Image>& image) {
  // 1 based image index
  auto index = find_or_append(images_, image);

  AddInt(index);
}

void RecordPlayback::AddTextBlob(const std::shared_ptr<TextBlob>& blob) {
  // 1 based blob index
  auto index = find_or_append(text_blobs_, blob) + 1;

  AddInt(index);
}

void RecordPlayback::Validate(size_t offset, size_t size) const {
  if (writer32_.BytesWritten() != offset + size) {
    std::cerr << "RecordPlayback::Validate failed: offset = " << offset
              << ", size = " << size << std::endl;
    std::cerr << "RecordPlayback::Validate failed: writer32_.BytesWritten() = "
              << writer32_.BytesWritten() << std::endl;

    std::abort();
  }
}

bool RecordPlayback::ParseStreamTag(ReadStream& stream, uint32_t tag,
                                    uint32_t size, TypefaceSet* typeface_set,
                                    int32_t recursion_limit) {
  switch (tag) {
    case SK_PICT_READER_TAG: {
      if (size == 0) {
        return false;
      }

      op_data_ = core_bridge::MakeDataFromMalloc(std::malloc(size), size);

      auto ptr = op_data_->RawData();

      if (!stream.Read(const_cast<void*>(ptr), size)) {
        op_data_ = nullptr;
        return false;
      }
    } break;

    case SK_PICT_FACTORY_TAG: {
      uint32_t factory_count = 0;
      if (!stream.ReadU32(&factory_count)) {
        return false;
      }

      for (uint32_t i = 0; i < factory_count; i++) {
        size_t len = 0;
        if (!stream.ReadPackedUint(&len)) {
          return false;
        }

        std::vector<char> factory_name(len, 0);

        if (stream.Read(factory_name.data(), len) != len) {
          return false;
        }

        playback_factory_set_.AddFactory(std::string(factory_name.data(), len));
      }
    } break;
    case SK_PICT_TYPEFACE_TAG: {
      for (uint32_t i = 0; i < size; i++) {
        if (stream.IsAtEnd()) {
          return false;
        }

        auto typeface = io::TypefaceMakeFromStream(stream);

        if (!typeface) {
          // FontStyle::Normal() == weight 400 / width 5 / upright.
          skity_font_style style = {400, 5, SKITY_FONT_SLANT_UPRIGHT};
          skity_font_manager fm = skity_font_manager_ref_default();
          skity_typeface tf =
              fm != nullptr ? skity_font_manager_get_default_typeface(fm, style)
                            : nullptr;
          skity_font_manager_destroy(fm);
          typeface = skity::capi::get_impl<skity_typeface_s, Typeface>(
              tf, SKITY_OBJECT_TYPE_TYPEFACE);
          skity_typeface_destroy(tf);
        }

        if (!typeface) {
          return false;
        }

        typeface_set->AddTypeface(typeface);
      }
    } break;
    case SK_PICT_PICTURE_TAG: {
      for (uint32_t i = 0; i < size; i++) {
        auto pic =
            Picture::MakeFromStream(stream, typeface_set, recursion_limit - 1);

        if (!pic) {
          return false;
        }

        sub_pictures_.emplace_back(std::move(pic));
      }
    } break;

    case SK_PICT_BUFFER_SIZE_TAG: {
      auto buffer_data =
          core_bridge::MakeDataFromMalloc(std::malloc(size), size);

      if (stream.Read(const_cast<void*>(buffer_data->RawData()), size) !=
          size) {
        return false;
      }

      ReadBuffer read_buffer(buffer_data->RawData(), size);

      read_buffer.SetVersion(target_version_);

      read_buffer.SetFactorySet(&playback_factory_set_);
      read_buffer.SetTypefaceSet(typeface_set);

      while (!read_buffer.IsEOF() && read_buffer.IsValid()) {
        tag = read_buffer.ReadU32();
        size = read_buffer.ReadU32();

        ParseBufferTag(read_buffer, tag, size);
      }

      if (!read_buffer.IsValid()) {
        return false;
      }
    } break;
  }

  return true;
}

bool RecordPlayback::ParseBufferTag(ReadBuffer& read_buffer, uint32_t tag,
                                    uint32_t size) {
  switch (tag) {
    case SK_PICT_PAINT_BUFFER_TAG: {
      auto count = size;

      for (uint32_t i = 0; i < count; i++) {
        auto paint = read_buffer.ReadPaint();

        if (!read_buffer.Validate(paint != nullptr)) {
          return false;
        }

        paints_.emplace_back(std::move(paint));
      }
    } break;
    case SK_PICT_PATH_BUFFER_TAG: {
      if (size > 0) {
        auto count = read_buffer.ReadInt();

        if (!read_buffer.Validate(count > 0)) {
          return false;
        }

        for (int32_t i = 0; i < count; i++) {
          auto path = read_buffer.ReadPath();

          if (!read_buffer.Validate(path.has_value())) {
            return false;
          }

          paths_.emplace_back(path.value());
        }
      }
    } break;
    case SK_PICT_TEXTBLOB_BUFFER_TAG: {
      parse_array_from_buffer(
          read_buffer, size, text_blobs_, [](ReadBuffer& buffer) {
            return ReadFromMemory<std::shared_ptr<TextBlob>>(buffer);
          });
    } break;
    case SK_PICT_SLUG_BUFFER_TAG:
      // do not support this tag for now
      return false;
    case SK_PICT_VERTICES_BUFFER_TAG: {
      skip_array_from_buffer<Vertices>(read_buffer, size);
    } break;
    case SK_PICT_IMAGE_BUFFER_TAG: {
      parse_array_from_buffer(
          read_buffer, size, images_,
          [](ReadBuffer& buffer) { return buffer.ReadImage(); });
    } break;
    case SK_PICT_READER_TAG: {
      if (!read_buffer.ValidateCanReadN<uint8_t>(size)) {
        return false;
      }

      auto reader_data =
          core_bridge::MakeDataFromMalloc(std::malloc(size), size);
      if (!read_buffer.ReadArrayN<uint8_t>(
              const_cast<void*>(reader_data->RawData()), size)) {
        return false;
      }

      op_data_ = reader_data;
    } break;

    case SK_PICT_PICTURE_TAG: {
      for (uint32_t i = 0; i < size; i++) {
        SkipPictureInBuffer(read_buffer);

        if (!read_buffer.IsValid()) {
          return false;
        }
      }
    } break;
    case SK_PICT_DRAWABLE_TAG: {
      // refuse pase picture with drawable data
      read_buffer.Validate(false);
      return false;
    } break;
    default:
      read_buffer.Validate(false);
      return false;
  }
  return true;
}

void RecordPlayback::ParseBuffer(ReadBuffer& buffer) {
  while (buffer.IsValid()) {
    auto tag = buffer.ReadU32();

    if (tag == SK_PICT_EOF_TAG) {
      break;
    }

    auto size = buffer.ReadU32();

    ParseBufferTag(buffer, tag, size);
  }

  buffer.Validate(op_data_ != nullptr);
}

const Path& RecordPlayback::GetPath(ReadBuffer& buffer) const {
  static Path kEmptyPath;

  auto index = buffer.ReadInt();

  return buffer.Validate(index > 0 && index <= paths_.size())
             ? paths_[index - 1]
             : kEmptyPath;
}

const Paint* RecordPlayback::RequiredPaint(ReadBuffer& buffer) const {
  auto* paint = OptionalPaint(buffer);
  return buffer.Validate(paint != nullptr) ? paint : nullptr;
}

const Paint* RecordPlayback::OptionalPaint(ReadBuffer& buffer) const {
  auto index = buffer.ReadInt();

  if (index == 0) {
    return nullptr;
  }

  return buffer.Validate(index > 0 && index <= paints_.size())
             ? paints_[index - 1].get()
             : nullptr;
}

const std::shared_ptr<Image>& RecordPlayback::GetImage(
    ReadBuffer& buffer) const {
  static std::shared_ptr<Image> kEmptyImage = {};

  auto index = buffer.ReadInt();

  return buffer.Validate(index >= 0 && index < images_.size()) ? images_[index]
                                                               : kEmptyImage;
}

const std::shared_ptr<TextBlob>& RecordPlayback::GetTextBlob(
    ReadBuffer& buffer) const {
  static std::shared_ptr<TextBlob> kEmptyBlob = {};

  auto index = buffer.ReadInt();

  return buffer.Validate(index > 0 && index <= text_blobs_.size())
             ? text_blobs_[index - 1]
             : kEmptyBlob;
}

}  // namespace skity
