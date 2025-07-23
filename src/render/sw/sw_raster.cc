// Copyright 2006 The Android Open Source Project.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/sw/sw_raster.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <skity/geometry/stroke.hpp>

#include "src/logging.hpp"
#include "src/tracing.hpp"

namespace skity {

void RealSpanBuilder::BuildSpan(int x, int y, const uint8_t alpha) {
  if (y < scan_bounds_.Top()) {
    return;
  }
  BuildSpan(x, y, 1, alpha);
}

void RealSpanBuilder::BuildSpan(int x, int y, int width, const uint8_t alpha) {
  if (y < scan_bounds_.Top()) {
    return;
  }

  if (delegate_) {
    delegate_->OnBuildSpan(x, y, width, alpha);
  }

  Span span{
      .x = x,
      .y = y,
      .len = width,
      .cover = alpha,
  };
  spans_.push_back(span);
}

void RealSpanBuilder::BuildSpans(int x, int y, const uint8_t antialias[],
                                 int len) {
  if (y < scan_bounds_.Top()) {
    return;
  }
  for (int i = 0; i < len; i++) {
    BuildSpan(x + i, y, 1, antialias[i]);
  }
}

inline static Alpha safely_add_alpha(Alpha alpha, Alpha delta) {
  if (static_cast<uint16_t>(alpha) + static_cast<uint16_t>(delta) > 255) {
    return 255;
  } else {
    return alpha + delta;
  }
}

void SpanBuilder::BuildSpan(int x, int y, const uint8_t alpha) {
  if (y < scan_bounds_.Top()) {
    return;
  }
  FlushYIfNeed(y);
  int offset = x - left_;
  alphas_[offset] = safely_add_alpha(alphas_[offset], alpha);
}

void SpanBuilder::BuildSpan(int x, int y, int width, const uint8_t alpha) {
  if (y < scan_bounds_.Top()) {
    return;
  }
  FlushYIfNeed(y);
  int offset = x - left_;
  for (int i = 0; i < width; i++) {
    alphas_[offset + i] = safely_add_alpha(alphas_[offset + i], alpha);
  }
}

void SpanBuilder::BuildSpans(int x, int y, const uint8_t antialias[], int len) {
  if (y < scan_bounds_.Top()) {
    return;
  }
  FlushYIfNeed(y);
  int offset = x - left_;
  for (int i = 0; i < len; i++) {
    alphas_[offset + i] = safely_add_alpha(alphas_[offset + i], antialias[i]);
  }
}

void SpanBuilder::FlushYIfNeed(int new_y) {
  if (curr_y_ == new_y) {
    return;
  }

  if (curr_y_ == SW_NaN32) {
    curr_y_ = new_y;
    return;
  }

  Flush();
  alphas_.assign(alphas_.size(), 0);
  curr_y_ = new_y;
}

void SpanBuilder::Flush() {
  if (curr_y_ == SW_NaN32) {
    return;
  }
  size_t curr = 0;
  size_t alpha_size = alphas_.size();
  while (curr < alpha_size) {
    if (alphas_[curr] > 0) {
      size_t start = curr;
      size_t width = 1;
      Alpha curr_alpha = alphas_[curr];
      do {
        curr++;
        if (curr >= alpha_size) {
          break;
        }

        if (alphas_[curr] == curr_alpha) {
          width++;
        } else {
          break;
        }
      } while (alphas_[curr]);
      real_span_builder_.BuildSpan(left_ + start, curr_y_, width, curr_alpha);
    } else {
      curr++;
    }
  }
}

// Return true if prev->fX, next->fX are too close in the current pixel row.
static bool edges_too_close(SWEdge* prev, SWEdge* next, SWFixed lowerY) {
  constexpr SWFixed SLACK = SW_Fixed1;
  return next && prev && next->upper_y < lowerY &&
         prev->x + SLACK >= next->x - std::abs(next->dx);
}

// This function exists for the case where the previous rite edge is removed
// because its fLowerY <= next_y
static bool edges_too_close(int prevRite, SWFixed ul, SWFixed ll) {
  return prevRite > SWFixedFloorToInt(ul) || prevRite > SWFixedFloorToInt(ll);
}

static Alpha get_partial_alpha(Alpha alpha, SWFixed partialHeight) {
  return static_cast<Alpha>(SWFixedRoundToInt(alpha * partialHeight));
}

static Alpha get_partial_alpha(Alpha alpha, Alpha fullAlpha) {
  return (alpha * fullAlpha) >> 8;
}

static void update_next_next_y(SWFixed y, SWFixed next_y,
                               SWFixed* next_next_y) {
  *next_next_y = y > next_y && y < *next_next_y ? y : *next_next_y;
}

static void check_intersection(const SWEdge* edge, SWFixed next_y,
                               SWFixed* next_next_y) {
  if (edge->prev->prev && edge->prev->x + edge->prev->dx > edge->x + edge->dx) {
    *next_next_y = next_y + (SW_Fixed1 >> SWEdge::kDefaultAccuracy);
  }
}

static inline void remove_edge(SWEdge* edge) {
  edge->prev->next = edge->next;
  edge->next->prev = edge->prev;
}

static inline void insert_edge_after(SWEdge* edge, SWEdge* after_me) {
  edge->prev = after_me;
  edge->next = after_me->next;
  after_me->next->prev = edge;
  after_me->next = edge;
}

void backward_insert_edge_based_on_x(SWEdge* edge) {
  SWFixed x = edge->x;
  SWEdge* prev = edge->prev;
  while (prev->prev && prev->x > x) {
    prev = prev->prev;
  }
  if (prev->next != edge) {
    remove_edge(edge);
    insert_edge_after(edge, prev);
  }
}

// Start from the right side, searching backwards for the point to begin the
// new edge list insertion, marching forwards from here. The implementation
// could have started from the left of the prior insertion, and search to the
// right, or with some additional caching, binary search the starting point.
// More work could be done to determine optimal new edge insertion.
SWEdge* backward_insert_start(SWEdge* prev, SWFixed x) {
  while (prev->prev && prev->x > x) {
    prev = prev->prev;
  }
  return prev;
}

static void insert_new_edges(SWEdge* newEdge, SWFixed y, SWFixed* next_next_y) {
  if (newEdge->upper_y > y) {
    update_next_next_y(newEdge->upper_y, y, next_next_y);
    return;
  }
  SWEdge* prev = newEdge->prev;
  if (prev->x <= newEdge->x) {
    while (newEdge->upper_y <= y) {
      check_intersection(newEdge, y, next_next_y);
      update_next_next_y(newEdge->lower_y, y, next_next_y);
      newEdge = newEdge->next;
    }
    update_next_next_y(newEdge->upper_y, y, next_next_y);
    return;
  }
  // find first x pos to insert
  SWEdge* start = backward_insert_start(prev, newEdge->x);
  // insert the lot, fixing up the links as we go
  do {
    SWEdge* next = newEdge->next;
    do {
      if (start->next == newEdge) {
        goto nextEdge;
      }
      SWEdge* after = start->next;
      if (after->x >= newEdge->x) {
        break;
      }
      DEBUG_CHECK(start != after);
      start = after;
    } while (true);
    remove_edge(newEdge);
    insert_edge_after(newEdge, start);
  nextEdge:
    check_intersection(newEdge, y, next_next_y);
    update_next_next_y(newEdge->lower_y, y, next_next_y);
    start = newEdge;
    newEdge = next;
  } while (newEdge->upper_y <= y);
  update_next_next_y(newEdge->upper_y, y, next_next_y);
}

static Alpha fixed_to_alpha(SWFixed f) { return get_partial_alpha(0xFF, f); }

// Suppose that line (l1, y)-(r1, y+1) intersects with (l2, y)-(r2, y+1),
// approximate (very coarsely) the x coordinate of the intersection.
static SWFixed approximate_intersection(SWFixed l1, SWFixed r1, SWFixed l2,
                                        SWFixed r2) {
  if (l1 > r1) {
    std::swap(l1, r1);
  }
  if (l2 > r2) {
    std::swap(l2, r2);
  }
  return (std::max(l1, l2) + std::min(r1, r2)) / 2;
}

// Return the alpha of a trapezoid whose height is 1
static Alpha trapezoid_to_alpha(SWFixed l1, SWFixed l2) {
  DEBUG_CHECK(l1 >= 0 && l2 >= 0);
  SWFixed area = (l1 + l2) / 2;
  return static_cast<Alpha>(area >> 8);
}

// The alpha of right-triangle (a, a*b)
static Alpha partial_triangle_to_alpha(SWFixed a, SWFixed b) {
  DEBUG_CHECK(a <= SW_Fixed1);
  // Approximating...
  // SWFixed area = SWFixedMul(a, SWFixedMul(a,b)) / 2;
  SWFixed area = (a >> 11) * (a >> 11) * (b >> 11);
  return static_cast<Alpha>((area >> 8) & 0xFF);
}

// Note that if fullAlpha != 0xFF, we'll multiply alpha by fullAlpha
static void blit_single_alpha(SpanBuilder* span_builder, int y, int x,
                              Alpha alpha, Alpha fullAlpha,
                              bool no_real_span_builder) {
  if (fullAlpha == 0xFF && !no_real_span_builder) {
    span_builder->GetRealSpanBuilder()->BuildSpan(x, y, alpha);
  } else {
    span_builder->BuildSpan(x, y, get_partial_alpha(alpha, fullAlpha));
  }
}

static void blit_two_alphas(SpanBuilder* span_builder, int y, int x, Alpha a1,
                            Alpha a2, Alpha fullAlpha,
                            bool no_real_span_builder) {
  if (fullAlpha == 0xFF && !no_real_span_builder) {
    span_builder->GetRealSpanBuilder()->BuildSpan(x, y, a1);
    span_builder->GetRealSpanBuilder()->BuildSpan(x + 1, y, a2);
  } else {
    span_builder->BuildSpan(x, y, a1);
    span_builder->BuildSpan(x + 1, y, a2);
  }
}

static void blit_full_alpha(SpanBuilder* span_builder, int y, int x, int len,
                            Alpha fullAlpha, bool no_real_span_builder) {
  if (fullAlpha == 0xFF && !no_real_span_builder) {
    span_builder->GetRealSpanBuilder()->BuildSpan(x, y, len, fullAlpha);
  } else {
    span_builder->BuildSpan(x, y, len, fullAlpha);
  }
}

// Here we always send in l < SW_Fixed1, and the first alpha we want to
// compute is alphas[0]
static void compute_alpha_above_line(Alpha* alphas, SWFixed l, SWFixed r,
                                     SWFixed dY, Alpha fullAlpha) {
  DEBUG_CHECK(l <= r);
  DEBUG_CHECK(l >> 16 == 0);
  int R = SWFixedCeilToInt(r);
  if (R == 0) {
    return;
  } else if (R == 1) {
    alphas[0] = get_partial_alpha(((R << 17) - l - r) >> 9, fullAlpha);
  } else {
    SWFixed first =
        SW_Fixed1 - l;  // horizontal edge length of the left-most triangle
    SWFixed last =
        r -
        ((R - 1) << 16);  // horizontal edge length of the right-most triangle
    SWFixed firstH =
        SWFixedMul(first, dY);  // vertical edge of the left-most triangle
    alphas[0] = SWFixedMul(first, firstH) >> 9;  // triangle alpha
    SWFixed alpha16 = firstH + (dY >> 1);        // rectangle plus triangle
    for (int i = 1; i < R - 1; ++i) {
      alphas[i] = alpha16 >> 8;
      alpha16 += dY;
    }
    alphas[R - 1] = fullAlpha - partial_triangle_to_alpha(last, dY);
  }
}

// Here we always send in l < SW_Fixed1, and the first alpha we want to
// compute is alphas[0]
static void compute_alpha_below_line(Alpha* alphas, SWFixed l, SWFixed r,
                                     SWFixed dY, Alpha fullAlpha) {
  DEBUG_CHECK(l <= r);
  DEBUG_CHECK(l >> 16 == 0);
  int R = SWFixedCeilToInt(r);
  if (R == 0) {
    return;
  } else if (R == 1) {
    alphas[0] = get_partial_alpha(trapezoid_to_alpha(l, r), fullAlpha);
  } else {
    SWFixed first =
        SW_Fixed1 - l;  // horizontal edge length of the left-most triangle
    SWFixed last =
        r -
        ((R - 1) << 16);  // horizontal edge length of the right-most triangle
    SWFixed lastH =
        SWFixedMul(last, dY);  // vertical edge of the right-most triangle
    alphas[R - 1] = SWFixedMul(last, lastH) >> 9;  // triangle alpha
    SWFixed alpha16 = lastH + (dY >> 1);           // rectangle plus triangle
    for (int i = R - 2; i > 0; i--) {
      alphas[i] = (alpha16 >> 8) & 0xFF;
      alpha16 += dY;
    }
    alphas[0] = fullAlpha - partial_triangle_to_alpha(first, dY);
  }
}

static void blit_aaa_trapezoid_row(SpanBuilder* span_builder, int y, SWFixed ul,
                                   SWFixed ur, SWFixed ll, SWFixed lr,
                                   SWFixed lDY, SWFixed rDY, Alpha fullAlpha,
                                   bool no_real_span_builder) {
  int L = SWFixedFloorToInt(ul), R = SWFixedCeilToInt(lr);
  int len = R - L;
  if (len == 1) {
    Alpha alpha = trapezoid_to_alpha(ur - ul, lr - ll);
    blit_single_alpha(span_builder, y, L, alpha, fullAlpha,
                      no_real_span_builder);
    return;
  }

  const int kQuickLen = 31;
  char quickMemory[(sizeof(Alpha) * 2 + sizeof(int16_t)) * (kQuickLen + 1)];
  Alpha* alphas;

  if (len <= kQuickLen) {
    alphas = reinterpret_cast<Alpha*>(quickMemory);
  } else {
    alphas = new Alpha[(len + 1) * (sizeof(Alpha) * 2 + sizeof(int16_t))];
  }

  Alpha* tempAlphas = alphas + len + 1;
  int16_t* runs = reinterpret_cast<int16_t*>((alphas + (len + 1) * 2));

  for (int i = 0; i < len; ++i) {
    runs[i] = 1;
    alphas[i] = fullAlpha;
  }
  runs[len] = 0;

  int uL = SWFixedFloorToInt(ul);
  int lL = SWFixedCeilToInt(ll);
  if (uL + 2 == lL) {  // We only need to compute two triangles, accelerate
                       // this special case
    SWFixed first = SWIntToFixed(uL) + SW_Fixed1 - ul;
    SWFixed second = ll - ul - first;
    Alpha a1 = fullAlpha - partial_triangle_to_alpha(first, lDY);
    Alpha a2 = partial_triangle_to_alpha(second, lDY);
    alphas[0] = alphas[0] > a1 ? alphas[0] - a1 : 0;
    alphas[1] = alphas[1] > a2 ? alphas[1] - a2 : 0;
  } else {
    compute_alpha_below_line(tempAlphas + uL - L, ul - SWIntToFixed(uL),
                             ll - SWIntToFixed(uL), lDY, fullAlpha);
    for (int i = uL; i < lL; ++i) {
      if (alphas[i - L] > tempAlphas[i - L]) {
        alphas[i - L] -= tempAlphas[i - L];
      } else {
        alphas[i - L] = 0;
      }
    }
  }

  int uR = SWFixedFloorToInt(ur);
  int lR = SWFixedCeilToInt(lr);
  if (uR + 2 == lR) {  // We only need to compute two triangles, accelerate
                       // this special case
    SWFixed first = SWIntToFixed(uR) + SW_Fixed1 - ur;
    SWFixed second = lr - ur - first;
    Alpha a1 = partial_triangle_to_alpha(first, rDY);
    Alpha a2 = fullAlpha - partial_triangle_to_alpha(second, rDY);
    alphas[len - 2] = alphas[len - 2] > a1 ? alphas[len - 2] - a1 : 0;
    alphas[len - 1] = alphas[len - 1] > a2 ? alphas[len - 1] - a2 : 0;
  } else {
    compute_alpha_above_line(tempAlphas + uR - L, ur - SWIntToFixed(uR),
                             lr - SWIntToFixed(uR), rDY, fullAlpha);
    for (int i = uR; i < lR; ++i) {
      if (alphas[i - L] > tempAlphas[i - L]) {
        alphas[i - L] -= tempAlphas[i - L];
      } else {
        alphas[i - L] = 0;
      }
    }
  }

  if (fullAlpha == 0xFF && !no_real_span_builder) {
    span_builder->GetRealSpanBuilder()->BuildSpans(L, y, alphas, len);
  } else {
    span_builder->BuildSpans(L, y, alphas, len);
  }

  if (len > kQuickLen) {
    delete[] alphas;
  }
}

static void blit_trapezoid_row(SpanBuilder* span_builder, int y, SWFixed ul,
                               SWFixed ur, SWFixed ll, SWFixed lr, SWFixed lDY,
                               SWFixed rDY, Alpha fullAlpha,
                               bool no_real_span_builder = false) {
  DEBUG_CHECK(lDY >= 0 &&
              rDY >= 0);  // We should only send in the absolte value

  // "ul > ur" is invalid
  if (ul > ur) {
    return;
  }

  // Edge crosses. Approximate it. This should only happend due to precision
  // limit, so the approximation could be very coarse.
  if (ll > lr) {
    ll = lr = approximate_intersection(ul, ll, ur, lr);
  }

  if (ul == ur && ll == lr) {
    return;  // empty trapzoid
  }

  // The swap here will not affect the final calculation result, but it can
  // simplify the logic. After the swap, ul <= ll, lr <= ur
  if (ul > ll) {
    std::swap(ul, ll);
  }
  if (ur > lr) {
    std::swap(ur, lr);
  }

  // ceil left bottm
  SWFixed joinLeft = SWFixedCeilToFixed(ll);
  // floor right top
  SWFixed joinRite = SWFixedFloorToFixed(ur);
  if (joinLeft <= joinRite) {  // There's a rect from joinLeft to joinRite
                               // that we can blit
    if (ul < joinLeft) {
      int len = SWFixedCeilToInt(joinLeft - ul);
      if (len == 1) {
        // In this case, ul and ll pass through the same pixel
        Alpha alpha = trapezoid_to_alpha(joinLeft - ul, joinLeft - ll);
        blit_single_alpha(span_builder, y, ul >> 16, alpha, fullAlpha,
                          no_real_span_builder);
      } else if (len == 2) {
        // In this case, ul and ll pass through two pixels, first is the size
        // from ul to ceil(ul), second is the size from floor(ll) to ll, and
        // then through these two values ​​​​and the slope, we can
        // calculate the triangle , further calculate the pixel area
        SWFixed first = joinLeft - SW_Fixed1 - ul;
        SWFixed second = ll - ul - first;
        Alpha a1 = partial_triangle_to_alpha(first, lDY);
        Alpha a2 = fullAlpha - partial_triangle_to_alpha(second, lDY);
        blit_two_alphas(span_builder, y, ul >> 16, a1, a2, fullAlpha,
                        no_real_span_builder);
      } else {
        blit_aaa_trapezoid_row(span_builder, y, ul, joinLeft, ll, joinLeft, lDY,
                               SW_MaxS32, fullAlpha, no_real_span_builder);
      }
    }
    if (joinLeft < joinRite) {
      blit_full_alpha(span_builder, y, SWFixedFloorToInt(joinLeft),
                      SWFixedFloorToInt(joinRite - joinLeft), fullAlpha,
                      no_real_span_builder);
    }
    if (lr > joinRite) {
      int len = SWFixedCeilToInt(lr - joinRite);
      if (len == 1) {
        Alpha alpha = trapezoid_to_alpha(ur - joinRite, lr - joinRite);
        blit_single_alpha(span_builder, y, joinRite >> 16, alpha, fullAlpha,
                          no_real_span_builder);
      } else if (len == 2) {
        SWFixed first = joinRite + SW_Fixed1 - ur;
        SWFixed second = lr - ur - first;
        Alpha a1 = fullAlpha - partial_triangle_to_alpha(first, rDY);
        Alpha a2 = partial_triangle_to_alpha(second, rDY);
        blit_two_alphas(span_builder, y, joinRite >> 16, a1, a2, fullAlpha,
                        no_real_span_builder);
      } else {
        blit_aaa_trapezoid_row(span_builder, y, joinRite, ur, joinRite, lr,
                               SW_MaxS32, rDY, fullAlpha, no_real_span_builder);
      }
    }
  } else {
    blit_aaa_trapezoid_row(span_builder, y, ul, ur, ll, lr, lDY, rDY, fullAlpha,
                           no_real_span_builder);
  }
}

void WalkEdges(SWEdge* prev_head, SWEdge* next_tail,
               Path::PathFillType fill_type, SpanBuilder* span_builder,
               int start_y, int stop_y, SWFixed left_clip, SWFixed right_clip) {
  prev_head->x = prev_head->upper_x = left_clip;
  next_tail->x = next_tail->upper_x = right_clip;
  // During each scan, the scanning line consists of two parallel lines. The
  // upper line is denoted by y, and the lower line by next_y. For the
  // subsequent scan, next_y and next_next_y form the two lines involved.
  SWFixed y = std::max(prev_head->next->upper_y, SWIntToFixed(start_y));
  SWFixed next_next_y = SW_MaxS32;
  {
    SWEdge* edge;
    for (edge = prev_head->next; edge->upper_y <= y; edge = edge->next) {
      edge->GoY(y);
      update_next_next_y(edge->lower_y, y, &next_next_y);
    }
    update_next_next_y(edge->upper_y, y, &next_next_y);
  }

  int winding_mask = fill_type == Path::PathFillType::kEvenOdd ? 1 : -1;

  while (true) {
    int w = 0;
    bool in_interval = false;
    SWFixed prev_x = prev_head->x;
    SWFixed next_y = std::min(next_next_y, SWFixedCeilToFixed(y + 1));
    SWEdge* curr_edge = prev_head->next;
    SWEdge* left_edge = prev_head;
    SWFixed left = left_clip;
    SWFixed left_dy = 0;
    // The x value of the previous right side. It is used to calculate whether
    // edges are too close
    int prev_right = SWFixedFloorToInt(left_clip);
    next_next_y = SW_MaxS32;
    // The y_shift here is used to calculate the x value during GoY(). If
    // next_y is 1, then x = x + dx, if it is not an integer, x = x + dx >>
    // y_shift
    int y_shift = 0;
    if ((next_y - y) & (SW_Fixed1 >> 2)) {  // next_y = y + 1/4
      y_shift = 2;
      next_y = y + (SW_Fixed1 >> 2);
    } else if ((next_y - y) & (SW_Fixed1 >> 1)) {  // next_y = y + 1/2
      y_shift = 1;
    }
    // now next_y may be y+1/4 or y+1/2 or y+1
    Alpha full_alpha = fixed_to_alpha(next_y - y);

    while (curr_edge->upper_y <= y) {
      DEBUG_CHECK(curr_edge->lower_y >= next_y);
      DEBUG_CHECK(curr_edge->y == y);
      w += curr_edge->winding;
      bool prev_in_interval = in_interval;
      in_interval = w & winding_mask;

      bool is_left = in_interval && !prev_in_interval;
      bool is_right = !in_interval && prev_in_interval;

      if (is_left) {
        left = std::max(curr_edge->x, left_clip);
        left_dy = curr_edge->dy;
        left_edge = curr_edge;
        curr_edge->GoY(next_y, y_shift);
      } else if (is_right) {
        SWFixed right = std::min(right_clip, curr_edge->x);
        curr_edge->GoY(next_y, y_shift);
        SWFixed next_left = std::max(left_clip, left_edge->x);
        SWFixed next_right = std::min(right_clip, curr_edge->x);
        SWFixed right_dy = curr_edge->dy;
        blit_trapezoid_row(
            span_builder, y >> 16, left, right, next_left, next_right, left_dy,
            right_dy, full_alpha,
            (full_alpha == 0xFF &&
             (edges_too_close(prev_right, left, left_edge->x) ||
              edges_too_close(curr_edge, curr_edge->next, next_y))));
        prev_right = SWFixedCeilToInt(std::max(right, curr_edge->x));
      } else {
        curr_edge->GoY(next_y, y_shift);
      }

      SWEdge* next = curr_edge->next;
      SWFixed new_x;

      while (curr_edge->lower_y <= next_y) {
        if (curr_edge->curve_count > 0) {
          SWQuadEdge* quadEdge = static_cast<SWQuadEdge*>(curr_edge);
          quadEdge->KeepContinuous();
          if (!quadEdge->UpdateQuad()) {
            break;
          }
        } else {
          break;
        }
      }
      DEBUG_CHECK(curr_edge->y == next_y);

      if (curr_edge->lower_y <= next_y) {
        remove_edge(curr_edge);
      } else {
        update_next_next_y(curr_edge->lower_y, next_y, &next_next_y);
        new_x = curr_edge->x;
        DEBUG_CHECK(curr_edge->lower_y > next_y);
        if (new_x < prev_x) {  // ripple currE backwards until it is x-sorted
          backward_insert_edge_based_on_x(curr_edge);
        } else {
          prev_x = new_x;
        }
        check_intersection(curr_edge, next_y, &next_next_y);
      }

      curr_edge = next;
      DEBUG_CHECK(curr_edge);
    }

    // was our right-edge culled away?
    if (in_interval) {
      blit_trapezoid_row(
          span_builder, y >> 16, left, right_clip,
          std::max(left_clip, left_edge->x), right_clip, left_dy, 0, full_alpha,
          full_alpha == 0xFF &&
              edges_too_close(left_edge->prev, left_edge, next_y));
    }

    y = next_y;
    if (y >= SWIntToFixed(stop_y)) {
      break;
    }

    // now currE points to the first edge with a fUpperY larger than the
    // previous y
    insert_new_edges(curr_edge, y, &next_next_y);
  }
}

static void SortEdges(std::vector<std::unique_ptr<SWEdge>>& edges) {
  std::sort(
      edges.begin(), edges.end(),
      [](const std::unique_ptr<SWEdge>& a, const std::unique_ptr<SWEdge>& b) {
        int valuea = a.get()->upper_y;
        int valueb = b.get()->upper_y;

        if (valuea == valueb) {
          valuea = a.get()->x;
          valueb = b.get()->x;
        }

        if (valuea == valueb) {
          valuea = a.get()->dx;
          valueb = b.get()->dx;
        }

        return valuea < valueb;
      });
  size_t count = edges.size();

  for (size_t i = 1; i < count; i++) {
    edges[i - 1]->next = edges[i].get();
    edges[i]->prev = edges[i - 1].get();
  }
}

static void ProcessEdges(std::vector<std::unique_ptr<SWEdge>>& edges,
                         SWEdge& head, SWEdge& tail) {
  SortEdges(edges);
  SWEdge* first = edges[0].get();
  SWEdge* last = edges.back().get();

  head.prev = nullptr;
  head.next = first;
  head.upper_y = head.lower_y = SW_MinS32;
  head.x = SW_MinS32;
  head.dx = 0;
  head.dy = SW_MaxS32;
  head.upper_x = SW_MinS32;
  first->prev = &head;

  tail.prev = last;
  tail.next = nullptr;
  tail.upper_y = tail.lower_y = SW_MaxS32;
  tail.x = SW_MaxS32;
  tail.dx = 0;
  tail.dy = SW_MaxS32;
  tail.upper_x = SW_MaxS32;
  last->next = &tail;
}

void SWRaster::RastePath(Path const& path, Matrix const& transform,
                         const Rect& clip_bounds,
                         SpanBuilderDelegate* span_builder_delegate) {
  SKITY_TRACE_EVENT(SWRaster_RastePath);

  Paint paint;
  Stroke stroke(paint);
  Path quad;
  stroke.QuadPath(path, &quad);
  Path transformed_path = quad.CopyWithMatrix(Matrix(transform));
  Rect scan_bounds = transformed_path.GetBounds();
  bounds_ = Rect::MakeLTRB(
      std::floor(scan_bounds.Left()), std::floor(scan_bounds.Top()),
      std::ceil(scan_bounds.Right()), std::ceil(scan_bounds.Bottom()));

  if (!scan_bounds.Intersect(clip_bounds)) {
    scan_bounds.SetEmpty();
  }
  scan_bounds = Rect::MakeLTRB(
      std::floor(scan_bounds.Left()), std::floor(scan_bounds.Top()),
      std::ceil(scan_bounds.Right()), std::ceil(scan_bounds.Bottom()));
  if (scan_bounds.IsEmpty()) {
    return;
  }

  SWEdgeBuilder builder;
  int count = builder.BuildEdges(transformed_path, scan_bounds);
  auto& edges = builder.GetEdges();
  if (count == 0) {
    return;
  }
  SWEdge head;
  SWEdge tail;
  ProcessEdges(edges, head, tail);

  int start_y;
  int stop_y;
  SWFixed left_bound;
  SWFixed right_bound;
  SpanBuilder span_builder(bounds_.Left(), bounds_.Width(), scan_bounds,
                           span_builder_delegate);

  // It makes more sense to start scanning from scan_bounds.Top(), but currently
  // WalkEdges does not support it. If we start from scan_bounds.Top(), the
  // order of edges needs to be rearranged, and the logic of updating quad
  // also needs to be modified.
  start_y = bounds_.Top();
  stop_y = scan_bounds.Bottom();
  left_bound = static_cast<uint32_t>(scan_bounds.Left()) << 16;
  right_bound = static_cast<uint32_t>(scan_bounds.Right()) << 16;

  WalkEdges(&head, &tail, path.GetFillType(), &span_builder, start_y, stop_y,
            left_bound, right_bound);
  span_builder.Flush();
  spans_ = span_builder.TakeSpans();
}

}  // namespace skity
