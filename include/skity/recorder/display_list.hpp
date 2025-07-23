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
  int32_t GetValue() const { return offset_; }
  bool IsValid() const { return offset_ >= 0; }

 private:
  explicit RecordedOpOffset(int32_t offset) : offset_(offset) {}
  int32_t offset_;
  friend class RecordingCanvas;
};

struct RecordedOp;

// Manages a buffer allocated with malloc.
class DisplayListStorage {
 public:
  DisplayListStorage() = default;
  DisplayListStorage(DisplayListStorage&&) = default;

  uint8_t* get() const { return ptr_.get(); }

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

 public:
  DisplayList();
  DisplayList(DisplayListStorage&& storage, size_t byte_count,
              uint32_t op_count, const Rect& bounds);
  ~DisplayList();

  bool Empty() const { return byte_count_ == 0; }
  void Draw(Canvas* canvas);
  void DisposeOps(uint8_t* ptr, uint8_t* end);
  uint32_t OpCount() const { return op_count_; }

  const Rect& GetBounds() const { return bounds_; }

  Paint* GetOpPaintByOffset(RecordedOpOffset offset);

 private:
  const DisplayListStorage storage_;
  size_t byte_count_ = 0;
  uint32_t op_count_ = 0u;
  Rect bounds_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_RECORDER_DISPLAY_LIST_HPP
