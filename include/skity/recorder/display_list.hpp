// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_RECORDER_DISPLAY_LIST_HPP
#define INCLUDE_SKITY_RECORDER_DISPLAY_LIST_HPP

#include <memory>
#include <skity/macros.hpp>
#include <skity/render/canvas.hpp>
#include <vector>

namespace skity {

class RecordedOpOffset {
 public:
  static RecordedOpOffset Make(int32_t offset) {
    return RecordedOpOffset(offset);
  }

  int32_t GetValue() const { return offset_; }
  bool IsValid() const { return offset_ >= 0; }

 private:
  explicit RecordedOpOffset(int32_t offset) : offset_(offset) {}
  int32_t offset_;
  friend class RecordingCanvas;
  friend class DisplayList;
};

struct RecordedOp;
struct DisplayListBuilder;
class DisplayListRTree;

// Manages a buffer allocated with malloc.
class DisplayListStorage {
 public:
  DisplayListStorage() = default;
  DisplayListStorage(DisplayListStorage&&) = default;

  uint8_t* get() { return ptr_.get(); }
  const uint8_t* get() const { return ptr_.get(); }

  void realloc(size_t count) {
    ptr_.reset(static_cast<uint8_t*>(std::realloc(ptr_.release(), count)));
  }

 private:
  struct FreeDeleter {
    void operator()(uint8_t* p) { std::free(p); }
  };
  std::unique_ptr<uint8_t, FreeDeleter> ptr_;
};

class SKITY_API DisplayList {
  friend class RecordingCanvas;
  friend struct DisplayListBuilder;

 public:
  enum class Property : uint32_t {
    kNone = 0,
    kSaveLayer = 1 << 0,
    kShader = 1 << 1,
    kColorFilter = 1 << 2,
    kMaskFilter = 1 << 3,
    kImageFilter = 1 << 4,
  };

  DisplayList();
  DisplayList(DisplayListStorage&& storage, size_t byte_count,
              uint32_t op_count, const Rect& bounds, uint32_t properties);
  ~DisplayList();

  bool Empty() const { return byte_count_ == 0; }
  void Draw(Canvas* canvas) const;
  void Draw(Canvas* canvas, const Rect& cull_rect) const;
  void DisposeOps(uint8_t* ptr, uint8_t* end);
  uint32_t OpCount() const { return op_count_; }

  const Rect& GetBounds() const { return bounds_; }

  bool HasSaveLayer() const {
    return (properties_ & static_cast<uint32_t>(Property::kSaveLayer)) != 0;
  }

  bool HasShader() const {
    return (properties_ & static_cast<uint32_t>(Property::kShader)) != 0;
  }

  bool HasColorFilter() const {
    return (properties_ & static_cast<uint32_t>(Property::kColorFilter)) != 0;
  }

  bool HasMaskFilter() const {
    return (properties_ & static_cast<uint32_t>(Property::kMaskFilter)) != 0;
  }

  bool HasImageFilter() const {
    return (properties_ & static_cast<uint32_t>(Property::kImageFilter)) != 0;
  }

  Paint* GetOpPaintByOffset(RecordedOpOffset offset);
  std::vector<RecordedOpOffset> Search(const Rect& rect) const;
  std::vector<Rect> SearchNonOverlappingDrawnRects(const Rect& rect) const;

 private:
  void SetRTree(std::unique_ptr<DisplayListRTree> rtree);

  const DisplayListStorage storage_;
  size_t byte_count_ = 0;
  uint32_t op_count_ = 0u;
  Rect bounds_;
  uint32_t properties_ = 0;
  std::unique_ptr<DisplayListRTree> rtree_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_RECORDER_DISPLAY_LIST_HPP
