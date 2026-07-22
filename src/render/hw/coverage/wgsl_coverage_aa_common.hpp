// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_WGSL_COVERAGE_AA_COMMON_HPP
#define SRC_RENDER_HW_COVERAGE_WGSL_COVERAGE_AA_COMMON_HPP

namespace skity {

inline constexpr char kCoverageAAEdgeContributionWGSL[] = R"(
fn coverage_aa_y_at_x(line_from: vec2<f32>, line_to: vec2<f32>,
                      x: f32) -> f32 {
  return line_from.y + (x - line_from.x) *
                         ((line_to.y - line_from.y) /
                          (line_to.x - line_from.x));
}

fn coverage_aa_x_at_y(line_from: vec2<f32>, line_to: vec2<f32>,
                      y: f32) -> f32 {
  return line_from.x + (y - line_from.y) *
                         ((line_to.x - line_from.x) /
                          (line_to.y - line_from.y));
}

fn coverage_aa_edge_contribution(line_from: vec2<f32>, line_to: vec2<f32>,
                                 tile_pixel: vec2<f32>) -> f32 {
  let pixel_left: f32 = tile_pixel.x;
  let pixel_right: f32 = pixel_left + 1.0;
  let pixel_top: f32 = tile_pixel.y;
  let pixel_bottom: f32 = pixel_top + 1.0;
  let edge_top: f32 = min(line_from.y, line_to.y);
  let edge_bottom: f32 = max(line_from.y, line_to.y);
  let y_min: f32 = max(edge_top, pixel_top);
  let y_max: f32 = min(edge_bottom, pixel_bottom);
  // The CPU removes horizontal lines after fixed-point quantization, so this
  // only rejects pixels whose y interval does not overlap the edge.
  if y_min >= y_max {
    return 0.0;
  }

  let dx: f32 = line_to.x - line_from.x;
  let sign: f32 = select(-1.0, 1.0, line_to.y < line_from.y);
  if dx == 0.0 {
    let h: f32 = y_max - y_min;
    if line_from.x <= pixel_left {
      return sign * h;
    }
    if line_from.x >= pixel_right {
      return 0.0;
    }
    return sign * h * (pixel_right - line_from.x);
  }

  let line_px_left_y: f32 =
      clamp(coverage_aa_y_at_x(line_from, line_to, pixel_left), y_min, y_max);
  let line_px_right_y: f32 =
      clamp(coverage_aa_y_at_x(line_from, line_to, pixel_right), y_min, y_max);
  let h: f32 = abs(line_px_right_y - line_px_left_y);
  let line_px_left_x: f32 =
      coverage_aa_x_at_y(line_from, line_to, line_px_left_y);
  let line_px_right_x: f32 =
      coverage_aa_x_at_y(line_from, line_to, line_px_right_y);
  let area: f32 =
      h * (pixel_right - 0.5 * (line_px_left_x + line_px_right_x));
  let left_endpoint_y: f32 = select(line_to.y, line_from.y,
                                    line_from.x <= line_to.x);
  let cover: f32 = abs(line_px_left_y - clamp(left_endpoint_y, y_min, y_max));
  return sign * (cover + area);
}
)";

inline constexpr char kCoverageAAFillRuleWGSL[] = R"(
fn coverage_aa_resolve_alpha(winding: f32, even_odd: i32) -> f32 {
  var alpha: f32 = abs(winding);
  if even_odd != 0 {
    let parity: f32 = alpha - floor(alpha * 0.5) * 2.0;
    alpha = 1.0 - abs(1.0 - parity);
  }
  return clamp(alpha, 0.0, 1.0);
}
)";

// Keep the RGBA16Uint layout and 256 subpixel scale synchronized
// with CoverageAAEncodedLines and CoverageAATileLine.
inline constexpr char kCoverageAALineLoadWGSL[] = R"(
fn coverage_aa_load_line(line_index: u32, texture_width: u32) -> vec4<f32> {
  let coord: vec2<i32> = vec2<i32>(
      i32(line_index % texture_width), i32(line_index / texture_width));
  let encoded: vec4<u32> =
      textureLoad(uCoverageAALineTexture, coord, 0);
  return vec4<f32>(f32(encoded.x), f32(encoded.y),
                   f32(encoded.z), f32(encoded.w)) / 256.0;
}
)";

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_WGSL_COVERAGE_AA_COMMON_HPP
