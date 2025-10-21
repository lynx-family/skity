// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_INCLUDE_SKITY_IO_PICTURE_HPP
#define MODULE_IO_INCLUDE_SKITY_IO_PICTURE_HPP

#include <functional>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/image.hpp>
#include <skity/io/stream.hpp>
#include <skity/macros.hpp>
#include <skity/recorder/display_list.hpp>
#include <skity/text/typeface.hpp>
#include <string>

namespace skity {

class MemoryWriter32;
class RecordPlayback;

class ReadBuffer;

struct SKITY_API TypefaceSet {
  std::vector<std::shared_ptr<Typeface>> typefaces;

  int32_t AddTypeface(const std::shared_ptr<Typeface>& typeface);
};

/**
 * skia use function pointer and name mapping to create Flattenable object.
 * For ourself only needs to register the factory name.
 */
struct SKITY_API FactorySet {
  std::vector<std::string> factories;

  int32_t AddFactory(const std::string& factory);

  std::string GetFactoryName(int32_t index) const;

  size_t GetFactoryCount() const;
};

struct SKITY_API SerialProc {
  GPUContext* gpu_context = nullptr;

  std::function<std::shared_ptr<Pixmap>(const Image* image)> image_proc =
      nullptr;
};

/**
 * This class hoding the reocorded drawing commands.
 *
 * This class is used to serialize the drawing commands to a stream.
 * Or deserialize the drawing commands from a stream.
 */
class SKITY_API Picture final {
  friend class RecordPlayback;

 public:
  ~Picture();

  Picture(const Picture&) = delete;
  Picture& operator=(const Picture&) = delete;

  Rect GetCullRect() const { return cull_rect_; }

  static std::unique_ptr<Picture> MakeFromDisplayList(DisplayList* dl);

  static std::unique_ptr<Picture> MakeFromStream(ReadStream& stream);

  void Serialize(WriteStream& stream, const SerialProc* proc,
                 TypefaceSet* typeface_set = nullptr);

  void PlayBack(Canvas* canvas);

 private:
  Picture(std::unique_ptr<RecordPlayback> playback);

  static std::unique_ptr<Picture> MakeFromStream(ReadStream& stream,
                                                 TypefaceSet* typeface_set,
                                                 int32_t recursion_limit);

  void HandleOp(ReadBuffer& buffer, uint32_t type, size_t size, Canvas* canvas,
                const Matrix& matrix);

 private:
  std::unique_ptr<RecordPlayback> playback_;
  std::unique_ptr<MemoryWriter32> writer_ = {};

  Rect cull_rect_;
};

}  // namespace skity

#endif  // MODULE_IO_INCLUDE_SKITY_IO_PICTURE_HPP
