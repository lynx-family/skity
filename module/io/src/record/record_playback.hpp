// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_RECORD_RECORD_PLAYBACK_HPP
#define MODULE_IO_SRC_RECORD_RECORD_PLAYBACK_HPP

#include <skity/c/skity_canvas.h>

#include <skity/io/data.hpp>
#include <vector>

#include "src/io/memory_writer.hpp"
#include "src/picture.hpp"
#include "src/record/draw_type.hpp"
#include "src/stream/stream.hpp"

namespace skity {

#define MASK_24 0x00FFFFFF

#define PACK_8_24(small, large) ((small << 24) | large)

struct SerialProc;
struct TypefaceSet;

struct Vertices;

class ReadBuffer;

/*
 * Records a display-list op stream into the serialized picture format and
 * plays the recorded ops back onto a C-ABI canvas.
 *
 * Recording input arrives through the delegate canvas returned by
 * GetDelegateCanvas(): pass it to skity_display_list_draw (or call the
 * skity_canvas drawing entries on it) and the ops re-enter the On* members
 * below through the C callback table. Ops the table does not carry (lines,
 * circles, ovals, arcs, rounded rects) are converted to paths by the core
 * delegate before delivery — the same op granularity this recorder serializes.
 */
class RecordPlayback {
 public:
  RecordPlayback(uint32_t width, uint32_t height);
  RecordPlayback(uint32_t width, uint32_t height, int32_t target_version);

  ~RecordPlayback();

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

  // Delegate canvas receiving the recorded op stream. The handle stays owned
  // by this RecordPlayback and is destroyed with it; it is created lazily on
  // first call.
  skity_canvas GetDelegateCanvas();

  // Op entry points invoked by the delegate callback table. Types follow the
  // C callback surface (skity_clip_op carries the Canvas::ClipOp values).
  // Paints arrive as shared_ptr (borrowed from the delegate's wrapper) so the
  // recorder can retain them without cloning — Paint construction /
  // destruction stays on the C ABI side.
  void OnClipRect(Rect const& rect, skity_clip_op op);
  void OnClipPath(Path const& path, skity_clip_op op);
  void OnDrawRect(Rect const& rect, std::shared_ptr<Paint> paint);
  void OnDrawPath(Path const& path, std::shared_ptr<Paint> paint);
  void OnSaveLayer(const Rect& bounds, std::shared_ptr<Paint> paint);
  void OnDrawBlob(std::shared_ptr<TextBlob> blob, float x, float y,
                  std::shared_ptr<Paint> paint);
  void OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                       const Rect& dst, const SamplingOptions& sampling,
                       std::shared_ptr<Paint> paint);
  void OnDrawGlyphs(uint32_t count, const GlyphID glyphs[],
                    const float position_x[], const float position_y[],
                    std::shared_ptr<Font> font, std::shared_ptr<Paint> paint);
  void OnDrawPaint(std::shared_ptr<Paint> paint);
  void OnSave();
  void OnRestore();
  void OnTranslate(float dx, float dy);
  void OnScale(float sx, float sy);
  void OnRotate(float degree);
  void OnRotate(float degree, float px, float py);
  void OnSkew(float sx, float sy);
  void OnConcat(Matrix const& matrix);
  void OnSetMatrix(Matrix const& matrix);
  void OnResetMatrix();

  const Path& GetPath(ReadBuffer& buffer) const;
  const Paint* RequiredPaint(ReadBuffer& buffer) const;
  const Paint* OptionalPaint(ReadBuffer& buffer) const;

  const std::shared_ptr<Image>& GetImage(ReadBuffer& buffer) const;
  const std::shared_ptr<TextBlob>& GetTextBlob(ReadBuffer& buffer) const;

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

  void AddPaint(std::shared_ptr<Paint> paint);
  void AddPaintPtr(std::shared_ptr<Paint> paint);
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

  skity_canvas delegate_ = nullptr;

  std::vector<int32_t> restore_offset_stack_ = {};

  int32_t init_save_count = 0;

  std::vector<std::shared_ptr<Paint>> paints_ = {};
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
