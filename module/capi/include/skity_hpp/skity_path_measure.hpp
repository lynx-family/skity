// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_MEASURE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_MEASURE_HPP

// PathMeasure wrapper of the header-only RAII layer: samples a path by arc
// length — total length, position / tangent at a distance, and sub-paths
// between two distances. Multi-contour paths are measured one contour at a
// time (NextContour advances). See skity.hpp for the layer's design.

#include <skity_c/skity_path_measure.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_path.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Path-arc-length sampler (legacy skity::PathMeasure). */
class PathMeasure
    : public detail::OwnHandle<skity_path_measure, skity_path_measure_destroy> {
 public:
  /** Empty measure; bind a path later with SetPath. */
  PathMeasure() : OwnHandle(skity_path_measure_create(nullptr, 0u, 1.f)) {}

  /**
   * Measure @p path. @p res_scale > 1 increases measuring precision;
   * @p force_closed synthesizes a close when the contour is open.
   */
  explicit PathMeasure(const Path& path, bool force_closed = false,
                       float res_scale = 1.f)
      : OwnHandle(skity_path_measure_create(path.get(),
                                            force_closed ? 1u : 0u, res_scale)) {}

  /** Re-bind to @p path (an empty path detaches). */
  void SetPath(const Path& path, bool force_closed = false) {
    skity_path_measure_set_path(get(), path.get(), force_closed ? 1u : 0u);
  }

  /** Total length of the current contour, or 0 when no path is bound. */
  float GetLength() const { return skity_path_measure_get_length(get()); }

  /**
   * Sample the position and unit tangent at @p distance (clamped to
   * [0, length]); either output may be NULL. False when no path is bound or
   * the contour is zero-length.
   */
  bool GetPosTan(float distance, Point* out_position, Point* out_tangent) const {
    return skity_path_measure_get_pos_tan(get(), distance, out_position,
                                          out_tangent) != 0;
  }

  /**
   * Append the sub-path between @p start_d and @p stop_d to @p dst; false
   * for a zero-length segment or start_d > stop_d.
   */
  bool GetSegment(float start_d, float stop_d, Path& dst,
                  bool start_with_move_to = true) const {
    return skity_path_measure_get_segment(get(), start_d, stop_d, dst.get(),
                                          start_with_move_to ? 1u : 0u) != 0;
  }

  bool IsClosed() const { return skity_path_measure_is_closed(get()) != 0; }

  /** Advance to the next contour; false at the end. */
  bool NextContour() { return skity_path_measure_next_contour(get()) != 0; }
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_PATH_MEASURE_HPP
