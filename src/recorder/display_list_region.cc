// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/recorder/display_list_region.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#include "src/logging.hpp"

namespace skity {

namespace {

constexpr size_t kBinarySearchThreshold = 10u;

}  // namespace

DisplayListRegion::SpanBuffer::SpanBuffer(const SpanBuffer& other)
    : capacity_(other.capacity_), size_(other.size_) {
  if (capacity_ == 0) {
    return;
  }

  spans_ = static_cast<Span*>(std::malloc(capacity_ * sizeof(Span)));
  CHECK(spans_ != nullptr);
  std::memcpy(spans_, other.spans_, size_ * sizeof(Span));
}

DisplayListRegion::SpanBuffer::SpanBuffer(SpanBuffer&& other) noexcept
    : capacity_(other.capacity_), size_(other.size_), spans_(other.spans_) {
  other.capacity_ = 0;
  other.size_ = 0;
  other.spans_ = nullptr;
}

DisplayListRegion::SpanBuffer& DisplayListRegion::SpanBuffer::operator=(
    const SpanBuffer& other) {
  if (this == &other) {
    return *this;
  }

  SpanBuffer copy(other);
  std::swap(capacity_, copy.capacity_);
  std::swap(size_, copy.size_);
  std::swap(spans_, copy.spans_);
  return *this;
}

DisplayListRegion::SpanBuffer& DisplayListRegion::SpanBuffer::operator=(
    SpanBuffer&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  std::swap(capacity_, other.capacity_);
  std::swap(size_, other.size_);
  std::swap(spans_, other.spans_);
  return *this;
}

DisplayListRegion::SpanBuffer::~SpanBuffer() { std::free(spans_); }

void DisplayListRegion::SpanBuffer::Reserve(size_t capacity) {
  if (capacity_ >= capacity) {
    return;
  }

  auto* new_spans =
      static_cast<Span*>(std::realloc(spans_, capacity * sizeof(Span)));
  CHECK(new_spans != nullptr);
  spans_ = new_spans;
  capacity_ = capacity;
}

DisplayListRegion::SpanChunkHandle DisplayListRegion::SpanBuffer::StoreChunk(
    const Span* begin, const Span* end) {
  const size_t chunk_size = static_cast<size_t>(end - begin);
  const size_t min_capacity = size_ + chunk_size + 1u;
  CHECK(min_capacity <= std::numeric_limits<SpanChunkHandle>::max());
  if (capacity_ < min_capacity) {
    size_t new_capacity = std::max(min_capacity, capacity_ * 2u);
    new_capacity = std::max(new_capacity, static_cast<size_t>(512));
    Reserve(new_capacity);
  }

  const auto handle = static_cast<SpanChunkHandle>(size_);
  size_ = min_capacity;
  SetChunkSize(handle, chunk_size);
  std::memmove(spans_ + handle + 1u, begin, chunk_size * sizeof(Span));
  return handle;
}

size_t DisplayListRegion::SpanBuffer::GetChunkSize(
    SpanChunkHandle handle) const {
  DEBUG_CHECK(handle < size_);
  return static_cast<size_t>(spans_[handle].left);
}

void DisplayListRegion::SpanBuffer::SetChunkSize(SpanChunkHandle handle,
                                                 size_t size) {
  DEBUG_CHECK(handle < size_);
  DEBUG_CHECK(size <= static_cast<size_t>(std::numeric_limits<int32_t>::max()));
  DEBUG_CHECK(spans_ != nullptr);
  spans_[handle].left = static_cast<int32_t>(size);
  spans_[handle].right = 0;
}

void DisplayListRegion::SpanBuffer::GetSpans(SpanChunkHandle handle,
                                             const Span*& begin,
                                             const Span*& end) const {
  DEBUG_CHECK(handle < size_);
  begin = spans_ + handle + 1;
  end = begin + GetChunkSize(handle);
}

DisplayListRegion::DisplayListRegion(const std::vector<Rect>& rects) {
  SetRects(rects);
}

DisplayListRegion::DisplayListRegion(const Rect& rect) { SetRects({rect}); }

bool DisplayListRegion::SpansEqual(const SpanLine& line, const Span* begin,
                                   const Span* end) const {
  const Span *our_begin, *our_end;
  span_buffer_.GetSpans(line.chunk_handle, our_begin, our_end);
  return (our_end - our_begin) == (end - begin) &&
         std::equal(our_begin, our_end, begin);
}

DisplayListRegion::SpanLine DisplayListRegion::MakeLine(
    int32_t top, int32_t bottom, const std::vector<Span>& spans) {
  return MakeLine(top, bottom, spans.data(), spans.data() + spans.size());
}

DisplayListRegion::SpanLine DisplayListRegion::MakeLine(int32_t top,
                                                        int32_t bottom,
                                                        const Span* begin,
                                                        const Span* end) {
  return {top, bottom, span_buffer_.StoreChunk(begin, end)};
}

size_t DisplayListRegion::UnionLineSpans(std::vector<Span>& result,
                                         const SpanBuffer& a_buffer,
                                         SpanChunkHandle a_handle,
                                         const SpanBuffer& b_buffer,
                                         SpanChunkHandle b_handle) {
  class OrderedSpanAccumulator {
   public:
    explicit OrderedSpanAccumulator(std::vector<Span>& spans) : spans_(spans) {}

    void Accumulate(const Span& span) {
      if (len_ == 0 || span.left > last_right_) {
        spans_[len_++] = span;
        last_right_ = span.right;
      } else if (span.right > last_right_) {
        spans_[len_ - 1].right = span.right;
        last_right_ = span.right;
      }
    }

    size_t len_ = 0;

   private:
    std::vector<Span>& spans_;
    int32_t last_right_ = std::numeric_limits<int32_t>::min();
  };

  const Span *begin1, *end1;
  a_buffer.GetSpans(a_handle, begin1, end1);
  const Span *begin2, *end2;
  b_buffer.GetSpans(b_handle, begin2, end2);

  const size_t min_size =
      static_cast<size_t>(end1 - begin1) + static_cast<size_t>(end2 - begin2);
  if (result.size() < min_size) {
    result.resize(min_size);
  }

  OrderedSpanAccumulator accumulator(result);
  while (begin1 != end1 && begin2 != end2) {
    if (begin1->left < begin2->left) {
      accumulator.Accumulate(*begin1++);
    } else {
      accumulator.Accumulate(*begin2++);
    }
  }

  while (begin1 != end1) {
    accumulator.Accumulate(*begin1++);
  }
  while (begin2 != end2) {
    accumulator.Accumulate(*begin2++);
  }

  return accumulator.len_;
}

size_t DisplayListRegion::IntersectLineSpans(std::vector<Span>& result,
                                             const SpanBuffer& a_buffer,
                                             SpanChunkHandle a_handle,
                                             const SpanBuffer& b_buffer,
                                             SpanChunkHandle b_handle) {
  const Span *begin1, *end1;
  a_buffer.GetSpans(a_handle, begin1, end1);
  const Span *begin2, *end2;
  b_buffer.GetSpans(b_handle, begin2, end2);

  const size_t min_size = static_cast<size_t>(end1 - begin1) +
                          static_cast<size_t>(end2 - begin2) - 1u;
  if (result.size() < min_size) {
    result.resize(min_size);
  }

  Span* out = result.data();
  while (begin1 != end1 && begin2 != end2) {
    if (begin1->right <= begin2->left) {
      ++begin1;
    } else if (begin2->right <= begin1->left) {
      ++begin2;
    } else {
      const int32_t right = std::min(begin1->right, begin2->right);
      *out++ = {std::max(begin1->left, begin2->left), right};
      if (begin1->right == right) {
        ++begin1;
      }
      if (begin2->right == right) {
        ++begin2;
      }
    }
  }

  return static_cast<size_t>(out - result.data());
}

void DisplayListRegion::AppendLine(int32_t top, int32_t bottom,
                                   const Span* begin, const Span* end) {
  if (!lines_.empty() && lines_.back().bottom == top &&
      SpansEqual(lines_.back(), begin, end)) {
    lines_.back().bottom = bottom;
    return;
  }

  lines_.push_back(MakeLine(top, bottom, begin, end));
}

void DisplayListRegion::AppendLine(int32_t top, int32_t bottom,
                                   const SpanBuffer& buffer,
                                   SpanChunkHandle handle) {
  const Span *begin, *end;
  buffer.GetSpans(handle, begin, end);
  AppendLine(top, bottom, begin, end);
}

DisplayListRegion DisplayListRegion::MakeUnion(const DisplayListRegion& a,
                                               const DisplayListRegion& b) {
  if (a.IsEmpty()) {
    return b;
  }
  if (b.IsEmpty()) {
    return a;
  }
  if (a.IsSimple() && a.bounds_.Contains(b.bounds_)) {
    return a;
  }
  if (b.IsSimple() && b.bounds_.Contains(a.bounds_)) {
    return b;
  }

  DisplayListRegion result;
  result.bounds_ = a.bounds_;
  result.bounds_.Join(b.bounds_);
  result.span_buffer_.Reserve(a.span_buffer_.Capacity() +
                              b.span_buffer_.Capacity());
  result.lines_.reserve(a.lines_.size() + b.lines_.size());

  auto a_it = a.lines_.begin();
  auto b_it = b.lines_.begin();
  const auto a_end = a.lines_.end();
  const auto b_end = b.lines_.end();

  std::vector<Span> temp_spans;
  int32_t cur_top = std::numeric_limits<int32_t>::min();

  while (a_it != a_end && b_it != b_end) {
    const int32_t a_top = std::max(cur_top, a_it->top);
    const int32_t b_top = std::max(cur_top, b_it->top);
    if (a_it->bottom <= b_top) {
      result.AppendLine(a_top, a_it->bottom, a.span_buffer_,
                        a_it->chunk_handle);
      ++a_it;
    } else if (b_it->bottom <= a_top) {
      result.AppendLine(b_top, b_it->bottom, b.span_buffer_,
                        b_it->chunk_handle);
      ++b_it;
    } else if (a_top < b_top) {
      result.AppendLine(a_top, b_top, a.span_buffer_, a_it->chunk_handle);
      cur_top = b_top;
    } else if (b_top < a_top) {
      result.AppendLine(b_top, a_top, b.span_buffer_, b_it->chunk_handle);
      cur_top = a_top;
    } else {
      const int32_t new_bottom = std::min(a_it->bottom, b_it->bottom);
      const size_t size =
          UnionLineSpans(temp_spans, a.span_buffer_, a_it->chunk_handle,
                         b.span_buffer_, b_it->chunk_handle);
      result.AppendLine(a_top, new_bottom, temp_spans.data(),
                        temp_spans.data() + size);
      cur_top = new_bottom;
      if (cur_top == a_it->bottom) {
        ++a_it;
      }
      if (cur_top == b_it->bottom) {
        ++b_it;
      }
    }
  }

  while (a_it != a_end) {
    const int32_t a_top = std::max(cur_top, a_it->top);
    result.AppendLine(a_top, a_it->bottom, a.span_buffer_, a_it->chunk_handle);
    ++a_it;
  }

  while (b_it != b_end) {
    const int32_t b_top = std::max(cur_top, b_it->top);
    result.AppendLine(b_top, b_it->bottom, b.span_buffer_, b_it->chunk_handle);
    ++b_it;
  }

  return result;
}

DisplayListRegion DisplayListRegion::MakeIntersection(
    const DisplayListRegion& a, const DisplayListRegion& b) {
  if (!Rect::Intersect(a.bounds_, b.bounds_)) {
    return DisplayListRegion();
  }
  if (a.IsSimple() && b.IsSimple()) {
    Rect intersection = a.bounds_;
    return intersection.Intersect(b.bounds_) ? DisplayListRegion(intersection)
                                             : DisplayListRegion();
  }
  if (a.IsSimple() && a.bounds_.Contains(b.bounds_)) {
    return b;
  }
  if (b.IsSimple() && b.bounds_.Contains(a.bounds_)) {
    return a;
  }

  DisplayListRegion result;
  result.span_buffer_.Reserve(
      std::max(a.span_buffer_.Capacity(), b.span_buffer_.Capacity()));
  result.lines_.reserve(std::min(a.lines_.size(), b.lines_.size()));

  std::vector<SpanLine>::const_iterator a_it;
  std::vector<SpanLine>::const_iterator b_it;
  GetIntersectionIterators(a.lines_, b.lines_, a_it, b_it);

  const auto a_end = a.lines_.end();
  const auto b_end = b.lines_.end();
  std::vector<Span> temp_spans;
  int32_t cur_top = std::numeric_limits<int32_t>::min();

  while (a_it != a_end && b_it != b_end) {
    const int32_t a_top = std::max(cur_top, a_it->top);
    const int32_t b_top = std::max(cur_top, b_it->top);
    if (a_it->bottom <= b_top) {
      ++a_it;
    } else if (b_it->bottom <= a_top) {
      ++b_it;
    } else {
      const int32_t top = std::max(a_top, b_top);
      const int32_t bottom = std::min(a_it->bottom, b_it->bottom);
      const size_t size =
          IntersectLineSpans(temp_spans, a.span_buffer_, a_it->chunk_handle,
                             b.span_buffer_, b_it->chunk_handle);
      if (size > 0) {
        result.AppendLine(top, bottom, temp_spans.data(),
                          temp_spans.data() + size);
        Rect line_bounds = Rect::MakeLTRB(temp_spans.front().left, top,
                                          temp_spans[size - 1].right, bottom);
        if (result.bounds_.IsEmpty()) {
          result.bounds_ = line_bounds;
        } else {
          result.bounds_.Join(line_bounds);
        }
      }
      cur_top = bottom;
      if (cur_top == a_it->bottom) {
        ++a_it;
      }
      if (cur_top == b_it->bottom) {
        ++b_it;
      }
    }
  }

  return result;
}

std::vector<Rect> DisplayListRegion::GetRects(bool deband) const {
  std::vector<Rect> rects;
  if (IsEmpty()) {
    return rects;
  }
  if (IsSimple()) {
    rects.push_back(bounds_);
    return rects;
  }

  size_t rect_count = 0;
  size_t previous_span_end = 0;
  for (const auto& line : lines_) {
    rect_count += span_buffer_.GetChunkSize(line.chunk_handle);
  }
  rects.reserve(rect_count);

  for (const auto& line : lines_) {
    const Span *span_begin, *span_end;
    span_buffer_.GetSpans(line.chunk_handle, span_begin, span_end);
    for (const Span* span = span_begin; span < span_end; ++span) {
      int32_t top = line.top;
      if (deband) {
        auto iter = rects.begin() + static_cast<ptrdiff_t>(previous_span_end);
        while (iter != rects.begin()) {
          --iter;
          if (iter->Bottom() < static_cast<float>(top)) {
            break;
          }
          if (iter->Left() == span->left && iter->Right() == span->right) {
            top = static_cast<int32_t>(iter->Top());
            rects.erase(iter);
            --previous_span_end;
            break;
          }
        }
      }
      rects.push_back(
          Rect::MakeLTRB(span->left, top, span->right, line.bottom));
    }
    previous_span_end = rects.size();
  }

  return rects;
}

bool DisplayListRegion::Intersects(const Rect& rect) const {
  if (IsEmpty()) {
    return false;
  }
  Rect normalized = NormalizeRect(rect);
  if (normalized.IsEmpty()) {
    return false;
  }

  const bool bounds_intersect = Rect::Intersect(bounds_, normalized);
  if (IsSimple()) {
    return bounds_intersect;
  }
  if (!bounds_intersect) {
    return false;
  }

  const IRect query = ToIRect(normalized);
  auto it = lines_.begin();
  const auto end = lines_.end();
  if (lines_.size() > kBinarySearchThreshold &&
      it[kBinarySearchThreshold].bottom <= query.top) {
    it = std::lower_bound(
        lines_.begin() + static_cast<ptrdiff_t>(kBinarySearchThreshold + 1),
        lines_.end(), query.top,
        [](const SpanLine& line, int32_t top) { return line.bottom <= top; });
  } else {
    while (it != end && it->bottom <= query.top) {
      ++it;
    }
  }

  while (it != end && it->top < query.bottom) {
    const Span *begin, *span_end;
    span_buffer_.GetSpans(it->chunk_handle, begin, span_end);
    while (begin != span_end && begin->left < query.right) {
      if (begin->right > query.left) {
        return true;
      }
      ++begin;
    }
    ++it;
  }

  return false;
}

bool DisplayListRegion::Intersects(const DisplayListRegion& region) const {
  if (IsEmpty() || region.IsEmpty()) {
    return false;
  }
  const bool bounds_intersect = Rect::Intersect(bounds_, region.bounds_);
  const bool our_complex = IsComplex();
  const bool their_complex = region.IsComplex();
  if (!our_complex && !their_complex) {
    return bounds_intersect;
  }
  if (!bounds_intersect) {
    return false;
  }
  if (!our_complex) {
    return region.Intersects(bounds_);
  }
  if (!their_complex) {
    return Intersects(region.bounds_);
  }

  std::vector<SpanLine>::const_iterator ours;
  std::vector<SpanLine>::const_iterator theirs;
  GetIntersectionIterators(lines_, region.lines_, ours, theirs);
  const auto ours_end = lines_.end();
  const auto theirs_end = region.lines_.end();

  while (ours != ours_end && theirs != theirs_end) {
    if (ours->bottom <= theirs->top) {
      ++ours;
    } else if (theirs->bottom <= ours->top) {
      ++theirs;
    } else {
      const Span *ours_begin, *ours_span_end;
      span_buffer_.GetSpans(ours->chunk_handle, ours_begin, ours_span_end);
      const Span *theirs_begin, *theirs_span_end;
      region.span_buffer_.GetSpans(theirs->chunk_handle, theirs_begin,
                                   theirs_span_end);
      if (SpansIntersect(ours_begin, ours_span_end, theirs_begin,
                         theirs_span_end)) {
        return true;
      }
      if (ours->bottom < theirs->bottom) {
        ++ours;
      } else {
        ++theirs;
      }
    }
  }

  return false;
}

Rect DisplayListRegion::NormalizeRect(const Rect& rect) {
  Rect normalized = rect.MakeSorted();
  normalized.RoundOut();
  if (normalized.IsEmpty()) {
    return Rect::MakeEmpty();
  }
  return normalized;
}

DisplayListRegion::IRect DisplayListRegion::ToIRect(const Rect& rect) {
  return {static_cast<int32_t>(rect.Left()), static_cast<int32_t>(rect.Top()),
          static_cast<int32_t>(rect.Right()),
          static_cast<int32_t>(rect.Bottom())};
}

Rect DisplayListRegion::ToRect(const IRect& rect) {
  return Rect::MakeLTRB(rect.left, rect.top, rect.right, rect.bottom);
}

void DisplayListRegion::SetRects(const std::vector<Rect>& rects) {
  DEBUG_CHECK(lines_.empty());

  std::vector<IRect> normalized_rects;
  normalized_rects.reserve(rects.size());
  for (const auto& rect : rects) {
    Rect normalized = NormalizeRect(rect);
    if (normalized.IsEmpty()) {
      continue;
    }
    if (bounds_.IsEmpty()) {
      bounds_ = normalized;
    } else {
      bounds_.Join(normalized);
    }
    normalized_rects.push_back(ToIRect(normalized));
  }

  if (normalized_rects.empty()) {
    return;
  }

  std::vector<const IRect*> sorted_rects(normalized_rects.size());
  for (size_t i = 0; i < normalized_rects.size(); ++i) {
    sorted_rects[i] = &normalized_rects[i];
  }

  std::sort(sorted_rects.begin(), sorted_rects.end(),
            [](const IRect* a, const IRect* b) {
              if (a->top != b->top) {
                return a->top < b->top;
              }
              return a->left < b->left;
            });

  size_t active_end = 0;
  size_t next_rect = 0;
  int32_t cur_y = std::numeric_limits<int32_t>::min();
  std::vector<Span> working_spans;
  working_spans.reserve(sorted_rects.size());

  while (next_rect < sorted_rects.size() || active_end > 0) {
    size_t preserve_end = 0;
    for (size_t i = 0; i < active_end; ++i) {
      const IRect* rect = sorted_rects[i];
      if (rect->bottom > cur_y) {
        sorted_rects[preserve_end++] = rect;
      }
    }
    active_end = preserve_end;

    if (active_end == 0) {
      if (next_rect >= sorted_rects.size()) {
        break;
      }
      cur_y = sorted_rects[next_rect]->top;
    }

    while (next_rect < sorted_rects.size()) {
      const IRect* rect = sorted_rects[next_rect];
      if (rect->top > cur_y) {
        break;
      }

      ++next_rect;
      size_t insert_at = active_end++;
      while (insert_at > 0) {
        const IRect* active_rect = sorted_rects[insert_at - 1];
        if (active_rect->left <= rect->left) {
          break;
        }
        sorted_rects[insert_at] = active_rect;
        --insert_at;
      }
      sorted_rects[insert_at] = rect;
    }

    DEBUG_CHECK(active_end > 0);
    working_spans.clear();

    int32_t start_x = sorted_rects[0]->left;
    int32_t end_x = sorted_rects[0]->right;
    int32_t end_y = sorted_rects[0]->bottom;
    for (size_t i = 1; i < active_end; ++i) {
      const IRect* rect = sorted_rects[i];
      if (rect->left > end_x) {
        working_spans.push_back({start_x, end_x});
        start_x = rect->left;
        end_x = rect->right;
      } else if (rect->right > end_x) {
        end_x = rect->right;
      }
      if (rect->bottom < end_y) {
        end_y = rect->bottom;
      }
    }
    working_spans.push_back({start_x, end_x});

    // end_y must not pass by the top of the next input rect
    if (next_rect < sorted_rects.size() &&
        end_y > sorted_rects[next_rect]->top) {
      end_y = sorted_rects[next_rect]->top;
    }

    if (!lines_.empty() && lines_.back().bottom == cur_y &&
        SpansEqual(lines_.back(), working_spans.data(),
                   working_spans.data() + working_spans.size())) {
      lines_.back().bottom = end_y;
    } else {
      lines_.push_back(MakeLine(cur_y, end_y, working_spans));
    }
    cur_y = end_y;
  }
}

bool DisplayListRegion::IsComplex() const {
  return lines_.size() > 1 ||
         (lines_.size() == 1 &&
          span_buffer_.GetChunkSize(lines_.front().chunk_handle) > 1);
}

bool DisplayListRegion::SpansIntersect(const Span* begin1, const Span* end1,
                                       const Span* begin2, const Span* end2) {
  while (begin1 != end1 && begin2 != end2) {
    if (begin1->right <= begin2->left) {
      ++begin1;
    } else if (begin2->right <= begin1->left) {
      ++begin2;
    } else {
      return true;
    }
  }
  return false;
}

void DisplayListRegion::GetIntersectionIterators(
    const std::vector<SpanLine>& a_lines, const std::vector<SpanLine>& b_lines,
    std::vector<SpanLine>::const_iterator& a_it,
    std::vector<SpanLine>::const_iterator& b_it) {
  a_it = a_lines.begin();
  b_it = b_lines.begin();
  if (a_it == a_lines.end() || b_it == b_lines.end()) {
    return;
  }

  if (a_lines.size() > kBinarySearchThreshold &&
      a_lines[kBinarySearchThreshold].bottom <= b_it->top) {
    a_it = std::lower_bound(
        a_lines.begin() + static_cast<ptrdiff_t>(kBinarySearchThreshold + 1),
        a_lines.end(), b_it->top,
        [](const SpanLine& line, int32_t top) { return line.bottom <= top; });
  } else if (b_lines.size() > kBinarySearchThreshold &&
             b_lines[kBinarySearchThreshold].bottom <= a_it->top) {
    b_it = std::lower_bound(
        b_lines.begin() + static_cast<ptrdiff_t>(kBinarySearchThreshold + 1),
        b_lines.end(), a_it->top,
        [](const SpanLine& line, int32_t top) { return line.bottom <= top; });
  }
}

}  // namespace skity
