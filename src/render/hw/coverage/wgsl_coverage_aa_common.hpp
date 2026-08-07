// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_COVERAGE_WGSL_COVERAGE_AA_COMMON_HPP
#define SRC_RENDER_HW_COVERAGE_WGSL_COVERAGE_AA_COMMON_HPP

namespace skity {

inline constexpr char kCoverageAAEdgeContributionWGSL[] = R"(
fn coverage_aa_edge_contribution(line_from: vec2<f32>, line_to: vec2<f32>,
                                 tile_pixel: vec2<f32>) -> f32 {
  let pixel_left: f32 = tile_pixel.x;
  let pixel_top: f32 = tile_pixel.y;
  let pixel_right: f32 = pixel_left + 1.0;
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

  let delta: vec2<f32> = line_to - line_from;
  let sign: f32 = select(-1.0, 1.0, delta.y < 0.0);

  if delta.x == 0.0 {
    let h: f32 = y_max - y_min;
    let covered_width: f32 =
        clamp(pixel_right - line_from.x, 0.0, 1.0);
    return sign * h * covered_width;
  }

  let y_slope: f32 = delta.y / delta.x;
  let x_slope: f32 = delta.x / delta.y;
  let line_px_left_y: f32 =
      clamp(line_from.y + (pixel_left - line_from.x) * y_slope, y_min, y_max);
  let line_px_right_y: f32 =
      clamp(line_from.y + (pixel_right - line_from.x) * y_slope, y_min, y_max);
  let h: f32 = abs(line_px_right_y - line_px_left_y);
  let line_px_left_x: f32 =
      line_from.x + (line_px_left_y - line_from.y) * x_slope;
  let line_px_right_x: f32 =
      line_from.x + (line_px_right_y - line_from.y) * x_slope;
  let area: f32 =
      h * (pixel_right - 0.5 * (line_px_left_x + line_px_right_x));
  let left_endpoint_y: f32 = select(line_to.y, line_from.y,
                                    line_from.x <= line_to.x);
  let cover: f32 = abs(line_px_left_y - clamp(left_endpoint_y, y_min, y_max));
  return sign * (cover + area);
}
)";

inline constexpr char kCoverageAAFillRuleWGSL[] = R"(
fn coverage_aa_resolve_alpha(winding: f32, is_even_odd: bool) -> f32 {
  if is_even_odd {
    let even_winding: f32 = 2.0 * round(0.5 * winding);
    return min(abs(winding - even_winding), 1.0);
  }
  return min(abs(winding), 1.0);
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

inline constexpr char kCoverageAAResolveWGSL[] = R"(
fn coverage_aa_resolve_pixel(
    line_range: vec2<u32>, backdrop_and_fill_rule: vec2<i32>,
    tile_pixel: vec2<f32>) -> f32 {
  let line_start: u32 = line_range.x;
  let line_count: u32 = line_range.y;
  let backdrop: i32 = backdrop_and_fill_rule.x;
  let fill_rule: i32 = backdrop_and_fill_rule.y;
  let is_even_odd: bool = fill_rule != 0;
  if line_count == u32(0) {
    return coverage_aa_resolve_alpha(f32(backdrop), is_even_odd);
  }

  var winding: f32 = f32(backdrop);
  let line_texture_width: u32 = textureDimensions(uCoverageAALineTexture).x;
  for (var i: u32 = u32(0); i < line_count; i = i + u32(1)) {
    let line: vec4<f32> =
        coverage_aa_load_line(line_start + i, line_texture_width);
    winding = winding +
        coverage_aa_edge_contribution(line.xy, line.zw, tile_pixel);
  }
  return coverage_aa_resolve_alpha(winding, is_even_odd);
}
)";

inline constexpr char kCoverageAAConflationCorrectionWGSL[] = R"(
fn coverage_aa_sample_winding_delta(
    line_from: vec2<f32>, line_to: vec2<f32>,
    sample_x: vec4<f32>, sample_y: vec4<f32>) -> vec4<i32> {
  let dx: f32 = line_to.x - line_from.x;
  let dy: f32 = line_to.y - line_from.y;
  let y_min: f32 = min(line_from.y, line_to.y);
  let y_max: f32 = max(line_from.y, line_to.y);
  let after_top: vec4<i32> =
      select(vec4<i32>(0), vec4<i32>(1),
             sample_y >= vec4<f32>(y_min));
  let before_bottom: vec4<i32> =
      select(vec4<i32>(0), vec4<i32>(1),
             sample_y < vec4<f32>(y_max));

  // cross_value is dy * (intersection_x - sample_x). Multiplying by
  // -winding_delta normalizes both edge directions so <= 0 means that the
  // intersection belongs to the sample's leftward ray.
  let c: f32 = line_from.x * dy - dx * line_from.y;
  let cross_value: vec4<f32> =
      vec4<f32>(-dy) * sample_x + vec4<f32>(dx) * sample_y + vec4<f32>(c);
  let winding_delta: i32 = select(-1, 1, dy < 0.0);
  let intersects_left_ray: vec4<i32> =
      select(vec4<i32>(0), vec4<i32>(1),
             cross_value * vec4<f32>(f32(-winding_delta)) <= vec4<f32>(0.0));
  return after_top * before_bottom * intersects_left_ray *
         vec4<i32>(winding_delta);
}

fn coverage_aa_has_two_distinct_odd_windings(
    winding: vec4<i32>) -> bool {
  let parity: vec4<i32> = winding & vec4<i32>(1);

  // Rotating by one lane compares (x, y), (y, z), (z, w), and (w, x).
  let one_step_conflict: vec4<bool> = select(
      vec4<bool>(false), winding != winding.yzwx,
      (parity & parity.yzwx) != vec4<i32>(0));

  // Rotating by two lanes adds (x, z) and (y, w). The other two comparisons
  // repeat those pairs in reverse.
  let two_step_conflict: vec4<bool> = select(
      vec4<bool>(false), winding != winding.zwxy,
      (parity & parity.zwxy) != vec4<i32>(0));

  return any(one_step_conflict) || any(two_step_conflict);
}

fn coverage_aa_resolve_pixel_with_conflation_correction(
    line_range: vec2<u32>, backdrop_and_fill_rule: vec2<i32>,
    tile_pixel: vec2<f32>) -> f32 {
  let line_start: u32 = line_range.x;
  let line_count: u32 = line_range.y;
  let backdrop: i32 = backdrop_and_fill_rule.x;
  let fill_rule: i32 = backdrop_and_fill_rule.y;
  let is_even_odd: bool = fill_rule != 0;
  if line_count == u32(0) {
    return coverage_aa_resolve_alpha(f32(backdrop), is_even_odd);
  }

  // Standard four-sample rotated-grid pattern.
  let sample_offset_x: vec4<f32> = vec4<f32>(0.375, 0.875, 0.125, 0.625);
  let sample_offset_y: vec4<f32> = vec4<f32>(0.125, 0.375, 0.625, 0.875);
  let sample_x: vec4<f32> =
      vec4<f32>(tile_pixel.x) + sample_offset_x;
  let sample_y: vec4<f32> =
      vec4<f32>(tile_pixel.y) + sample_offset_y;
  var sample_winding: vec4<i32> = vec4<i32>(backdrop);
  // Keep the probes half a packed subpixel inside the pixel to avoid the
  // half-open ownership ambiguity on exact pixel boundaries.
  let corner_inset: f32 = 0.5 / 256.0;
  let corner_max: f32 = 1.0 - corner_inset;
  // Lanes are top-left, top-right, bottom-left, and bottom-right.
  let corner_x: vec4<f32> = vec4<f32>(tile_pixel.x) +
      vec4<f32>(corner_inset, corner_max, corner_inset, corner_max);
  let corner_y: vec4<f32> = vec4<f32>(tile_pixel.y) +
      vec4<f32>(corner_inset, corner_inset, corner_max, corner_max);
  var corner_winding: vec4<i32> = vec4<i32>(backdrop);
  var winding: f32 = f32(backdrop);
  let line_texture_width: u32 = textureDimensions(uCoverageAALineTexture).x;
  for (var i: u32 = u32(0); i < line_count; i = i + u32(1)) {
    let line: vec4<f32> =
        coverage_aa_load_line(line_start + i, line_texture_width);
    winding = winding +
        coverage_aa_edge_contribution(line.xy, line.zw, tile_pixel);
    sample_winding = sample_winding + coverage_aa_sample_winding_delta(
        line.xy, line.zw, sample_x, sample_y);
    corner_winding = corner_winding + coverage_aa_sample_winding_delta(
        line.xy, line.zw, corner_x, corner_y);
  }

  let analytical_alpha: f32 =
      coverage_aa_resolve_alpha(winding, is_even_odd);
  var sample_is_inside: vec4<bool> =
      sample_winding != vec4<i32>(0);
  if is_even_odd {
    sample_is_inside =
        (sample_winding & vec4<i32>(1)) != vec4<i32>(0);
  }
  let covered_samples: vec4<i32> =
      select(vec4<i32>(0), vec4<i32>(1), sample_is_inside);
  let sampled_alpha: f32 = 0.25 * f32(
      covered_samples.x + covered_samples.y +
      covered_samples.z + covered_samples.w);
  // This is a heuristic correction: the corners detect selected
  // fill-rule-specific winding conflicts, and the interior samples estimate
  // replacement coverage. It does not identify every conflation artifact.
  let non_zero_may_conflate: bool =
      any(corner_winding < vec4<i32>(0)) &&
      any(corner_winding > vec4<i32>(0));
  // Only use winding values observed at the corners. A wide min/max range
  // alone does not prove that two distinct filled EvenOdd regions meet here.
  let even_odd_may_conflate: bool =
      coverage_aa_has_two_distinct_odd_windings(corner_winding);
  let may_conflate: bool = select(
      non_zero_may_conflate, even_odd_may_conflate,
      is_even_odd);
  return select(analytical_alpha, sampled_alpha, may_conflate);
}
)";

}  // namespace skity

#endif  // SRC_RENDER_HW_COVERAGE_WGSL_COVERAGE_AA_COMMON_HPP
