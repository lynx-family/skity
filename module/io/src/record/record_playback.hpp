// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_RECORD_RECORD_PLAYBACK_HPP
#define MODULE_IO_SRC_RECORD_RECORD_PLAYBACK_HPP

#include <skity/io/data.hpp>
#include <skity/io/picture.hpp>
#include <skity/io/stream.hpp>
#include <skity/render/canvas.hpp>
#include <vector>

#include "src/io/memory_writer.hpp"
#include "src/record/draw_type.hpp"

namespace skity {

#define MASK_24 0x00FFFFFF

#define PACK_8_24(small, large) ((small << 24) | large)

struct SerialProc;
struct TypefaceSet;

struct Vertices;

class ReadBuffer;

class RecordPlayback : public Canvas {
 public:
  RecordPlayback(uint32_t width, uint32_t height);
  RecordPlayback(uint32_t width, uint32_t height, int32_t target_version);

  ~RecordPlayback() override = default;

  void BeginRecording();
  void EndRecording();

  void Serialize(WriteStream& stream, const SerialProc* proc,
                 TypefaceSet* typeface_set);

  static std::unique_ptr<RecordPlayback> CreateFromStream(
      const Rect& cull_rect, int32_t target_version, ReadStream& stream,
      TypefaceSet* typeface_set, int32_t recursion_limit);

  void ParseBuffer(ReadBuffer& buffer);

  const std::shared_ptr<Data>& GetOpData() const { return op_data_; }

  int32_t GetTargetVersion() const { return target_version_; }

  const Path& GetPath(ReadBuffer& buffer) const;
  const Paint& RequiredPaint(ReadBuffer& buffer) const;
  const Paint* OptionalPaint(ReadBuffer& buffer) const;

  const std::shared_ptr<Image>& GetImage(ReadBuffer& buffer) const;
  const std::shared_ptr<TextBlob>& GetTextBlob(ReadBuffer& buffer) const;

 protected:
  void OnClipRect(Rect const& rect, ClipOp op) override;
  void OnClipPath(Path const& path, ClipOp op) override;
  void OnDrawRect(Rect const& rect, Paint const& paint) override;
  void OnDrawPath(Path const& path, Paint const& paint) override;
  void OnSaveLayer(const Rect& bounds, const Paint& paint) override;
  void OnDrawBlob(const TextBlob* blob, float x, float y,
                  Paint const& paint) override;
  void OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                       const Rect& dst, const SamplingOptions& sampling,
                       Paint const* paint) override;
  void OnDrawGlyphs(uint32_t count, const GlyphID glyphs[],
                    const float position_x[], const float position_y[],
                    const Font& font, const Paint& paint) override;
  void OnDrawPaint(Paint const& paint) override;
  void OnSave() override;
  void OnRestore() override;
  void OnRestoreToCount(int saveCount) override {}
  void OnTranslate(float dx, float dy) override;
  void OnScale(float sx, float sy) override;
  void OnRotate(float degree) override;
  void OnRotate(float degree, float px, float py) override;
  void OnSkew(float sx, float sy) override;
  void OnConcat(Matrix const& matrix) override;
  void OnSetMatrix(Matrix const& matrix) override;
  void OnResetMatrix() override;
  void OnFlush() override {}
  uint32_t OnGetWidth() const override { return width_; }
  uint32_t OnGetHeight() const override { return height_; }
  void OnUpdateViewport(uint32_t width, uint32_t height) override {}

 private:
  bool ParseStream(ReadStream& stream, TypefaceSet* typeface_set,
                   int32_t recursion_limit);

  void FlattenToBuffer(BinaryWriteBuffer& buffer);

  void WriteFactories(WriteStream& stream, const FactorySet& factory_set);
  void WriteTypefaces(WriteStream& stream, const TypefaceSet& typeface_set);

  size_t RecordRestoreOffsetPlaceHolder();
  void FillRestoreOffsetPlaceholder(uint32_t restore_offset);

  size_t AddDraw(DrawType type, size_t& size);

  void AddInt(int32_t value);
  void AddFloat(float value);
  void AddMatrix(const Matrix& matrix);

  void AddRect(const Rect& rect);

  void AddPaint(const Paint& paint) { AddPaintPtr(&paint); }
  void AddPaintPtr(const Paint* paint);
  int32_t AddPath(const Path& path);

  void AddImage(const std::shared_ptr<Image>& image);

  void AddTextBlob(const std::shared_ptr<TextBlob>& blob);

  void Validate(size_t offset, size_t size) const;

  bool ParseStreamTag(ReadStream& stream, uint32_t tag, uint32_t size,
                      TypefaceSet* typeface_set, int32_t recursion_limit);

  bool ParseBufferTag(ReadBuffer& read_buffer, uint32_t tag, uint32_t size);

 private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  MemoryWriter32 writer32_;

  std::vector<int32_t> restore_offset_stack_ = {};

  int32_t init_save_count = 0;

  std::vector<Paint> paints_ = {};
  std::vector<Path> paths_ = {};
  std::vector<std::shared_ptr<Image>> images_;
  std::vector<std::shared_ptr<TextBlob>> text_blobs_;

  // deserialize data
  int32_t target_version_ = 0;
  TypefaceSet playback_typeface_set_ = {};
  FactorySet playback_factory_set_ = {};
  std::shared_ptr<Data> op_data_ = nullptr;

  std::vector<std::unique_ptr<Picture>> sub_pictures_ = {};
};

}  // namespace skity

#endif  // MODULE_IO_SRC_RECORD_RECORD_PLAYBACK_HPP
