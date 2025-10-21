// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/record/record_playback.hpp"

#include <cstdlib>
#include <iostream>
#include <skity/io/picture.hpp>

#include "src/io/flat/font_desc_flat.hpp"
#include "src/io/memory_read.hpp"
#include "src/io/read/read_typeface.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

static constexpr int32_t kUInt32Size = 4;

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

void RecordPlayback::BeginRecording() { init_save_count = Save(); }

void RecordPlayback::EndRecording() { RestoreToCount(init_save_count); }

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

void RecordPlayback::OnClipRect(Rect const& rect, ClipOp op) {
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

void RecordPlayback::OnClipPath(Path const& path, ClipOp op) {
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

void RecordPlayback::OnDrawRect(const Rect& rect, const Paint& paint) {
  // op + paint index + rect
  size_t size = 2 * kUInt32Size + sizeof(rect);

  auto offset = AddDraw(DrawType::DRAW_RECT, size);

  AddPaint(paint);
  AddRect(rect);

  Validate(offset, size);
}

void RecordPlayback::OnDrawPath(const Path& path, const Paint& paint) {
  // op + paint index + path index
  size_t size = 3 * kUInt32Size;

  auto offset = AddDraw(DrawType::DRAW_PATH, size);

  AddPaint(paint);

  auto path_id = AddPath(path);
  AddInt(path_id);

  Validate(offset, size);
}

void RecordPlayback::OnSaveLayer(const Rect& bounds, const Paint& paint) {
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

void RecordPlayback::OnDrawBlob(const TextBlob* blob, float x, float y,
                                Paint const& paint) {
  // op + paint index + blob index + x/y
  size_t size = 3 * kUInt32Size + sizeof(float) * 2;

  auto offset = AddDraw(DrawType::DRAW_TEXT_BLOB, size);

  auto copyed_blob = std::make_shared<TextBlob>(blob->GetTextRun());

  AddPaint(paint);
  AddTextBlob(copyed_blob);
  AddFloat(x);
  AddFloat(y);

  Validate(offset, size);
}

void RecordPlayback::OnDrawImageRect(std::shared_ptr<Image> image,
                                     const Rect& src, const Rect& dst,
                                     const SamplingOptions& sampling,
                                     Paint const* paint) {
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
                                  const float position_y[], const Font& font,
                                  const Paint& paint) {
  // not support
}

void RecordPlayback::OnDrawPaint(Paint const& paint) {
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
  auto m = Matrix::RotateDeg(degrees);

  OnConcat(m);
}

void RecordPlayback::OnRotate(float degree, float px, float py) {
  auto m = Matrix::RotateDeg(degree, {px, py});

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
      buffer.WritePaint(paint);
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

    auto desc = typeface->GetFontDescriptor();

    SerializeFontDescriptor(stream, desc);

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

void RecordPlayback::AddPaintPtr(const Paint* paint) {
  if (paint) {
    paints_.push_back(*paint);
    AddInt(paints_.size());
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

      op_data_ = Data::MakeFromMalloc(std::malloc(size), size);

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
          auto fm = FontManager::RefDefault();

          typeface = fm->GetDefaultTypeface(FontStyle::Normal());
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
      auto buffer_data = Data::MakeFromMalloc(std::malloc(size), size);

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

        if (!read_buffer.Validate(paint.has_value())) {
          return false;
        }

        paints_.emplace_back(paint.value());
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

      auto reader_data = Data::MakeFromMalloc(std::malloc(size), size);
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

const Paint& RecordPlayback::RequiredPaint(ReadBuffer& buffer) const {
  static Paint kEmptyPaint;

  auto paint = OptionalPaint(buffer);

  if (buffer.Validate(paint != nullptr)) {
    return *paint;
  }

  return kEmptyPaint;
}

const Paint* RecordPlayback::OptionalPaint(ReadBuffer& buffer) const {
  auto index = buffer.ReadInt();

  if (index == 0) {
    return nullptr;
  }

  return buffer.Validate(index > 0 && index <= paints_.size())
             ? &paints_[index - 1]
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
