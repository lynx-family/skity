// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RECORDER_DISPLAY_LIST_REGION_HPP
#define SRC_RECORDER_DISPLAY_LIST_REGION_HPP

#include <cstddef>
#include <cstdint>
#include <skity/geometry/rect.hpp>
#include <vector>

namespace skity {

class DisplayListRegion {
 public:
  DisplayListRegion() = default;
  explicit DisplayListRegion(const std::vector<Rect>& rects);
  explicit DisplayListRegion(const Rect& rect);

  static DisplayListRegion MakeUnion(const DisplayListRegion& a,
                                     const DisplayListRegion& b);
  static DisplayListRegion MakeIntersection(const DisplayListRegion& a,
                                            const DisplayListRegion& b);

  std::vector<Rect> GetRects(bool deband = true) const;

  const Rect& GetBounds() const { return bounds_; }
  bool IsEmpty() const { return lines_.empty(); }
  bool Intersects(const Rect& rect) const;
  bool Intersects(const DisplayListRegion& region) const;

 private:
  struct IRect {
    int32_t left = 0;
    int32_t top = 0;
    int32_t right = 0;
    int32_t bottom = 0;

    bool IsEmpty() const { return !(left < right && top < bottom); }
  };

  struct Span {
    int32_t left = 0;
    int32_t right = 0;

    bool operator==(const Span& other) const {
      return left == other.left && right == other.right;
    }
  };

  using SpanChunkHandle = uint32_t;

  class SpanBuffer {
   public:
    SpanBuffer() = default;
    SpanBuffer(const SpanBuffer& other);
    SpanBuffer(SpanBuffer&& other) noexcept;
    SpanBuffer& operator=(const SpanBuffer& other);
    SpanBuffer& operator=(SpanBuffer&& other) noexcept;
    ~SpanBuffer();

    void Clear() { size_ = 0; }
    void Reserve(size_t capacity);
    size_t Capacity() const { return capacity_; }

    SpanChunkHandle StoreChunk(const Span* begin, const Span* end);
    size_t GetChunkSize(SpanChunkHandle handle) const;
    void GetSpans(SpanChunkHandle handle, const Span*& begin,
                  const Span*& end) const;

   private:
    void SetChunkSize(SpanChunkHandle handle, size_t size);

    size_t capacity_ = 0;
    size_t size_ = 0;
    Span* spans_ = nullptr;
  };

  struct SpanLine {
    int32_t top = 0;
    int32_t bottom = 0;
    SpanChunkHandle chunk_handle = 0;
  };

  static Rect NormalizeRect(const Rect& rect);
  static IRect ToIRect(const Rect& rect);
  static Rect ToRect(const IRect& rect);

  void SetRects(const std::vector<Rect>& rects);
  void AppendLine(int32_t top, int32_t bottom, const Span* begin,
                  const Span* end);
  void AppendLine(int32_t top, int32_t bottom, const SpanBuffer& buffer,
                  SpanChunkHandle handle);

  SpanLine MakeLine(int32_t top, int32_t bottom,
                    const std::vector<Span>& spans);
  SpanLine MakeLine(int32_t top, int32_t bottom, const Span* begin,
                    const Span* end);

  bool IsComplex() const;
  bool IsSimple() const { return !IsComplex(); }
  bool SpansEqual(const SpanLine& line, const Span* begin,
                  const Span* end) const;

  static size_t UnionLineSpans(std::vector<Span>& result,
                               const SpanBuffer& a_buffer,
                               SpanChunkHandle a_handle,
                               const SpanBuffer& b_buffer,
                               SpanChunkHandle b_handle);
  static size_t IntersectLineSpans(std::vector<Span>& result,
                                   const SpanBuffer& a_buffer,
                                   SpanChunkHandle a_handle,
                                   const SpanBuffer& b_buffer,
                                   SpanChunkHandle b_handle);
  static bool SpansIntersect(const Span* begin1, const Span* end1,
                             const Span* begin2, const Span* end2);
  static void GetIntersectionIterators(
      const std::vector<SpanLine>& a_lines,
      const std::vector<SpanLine>& b_lines,
      std::vector<SpanLine>::const_iterator& a_it,
      std::vector<SpanLine>::const_iterator& b_it);

  std::vector<SpanLine> lines_;
  Rect bounds_ = Rect::MakeEmpty();
  SpanBuffer span_buffer_;
};

}  // namespace skity

#endif  // SRC_RECORDER_DISPLAY_LIST_REGION_HPP
