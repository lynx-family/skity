// Copyright (c) 2011 Google Inc. All rights reserved.

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/wgx_programmable_blending.hpp"

#include <sstream>

namespace skity {

namespace {
void AppendHelperFunction(BlendMode blend_mode, std::stringstream& ss) {
  switch (blend_mode) {
    case BlendMode::kOverlay:
    case BlendMode::kHardLight: {
      ss << R"(
fn blend_overlay_component(s: vec2<f32>, d: vec2<f32>) -> f32 {
  if 2.0 * d.x <= d.y {
    return 2.0 * s.x * d.x;
  } else {
    return s.y * d.y - 2.0 * (d.y - d.x) * (s.y - s.x);
  }
}
)";
      break;
    }
    case BlendMode::kColorDodge: {
      ss << R"(
fn blend_color_dodge_component(s: vec2<f32>, d: vec2<f32>) -> f32 {
  let kMinNormalHalf: f32 = 0.0001;
  var dx_scale: f32 =  select(1.0, 0.0, d.x == 0.0);
  var delta: f32 = dx_scale * min(d.y, select(d.y, (d.x * s.y) / (s.y - s.x), abs(s.y - s.x) >= kMinNormalHalf));
  return delta * s.y + s.x * (1.0 - d.y) + d.x * (1.0 - s.y);
}
)";
    } break;

    case BlendMode::kColorBurn: {
      ss << R"(
fn blend_color_burn_component(s: vec2<f32>, d: vec2<f32>) -> f32 {
  let kMinNormalHalf: f32 = 0.0001;
  var dy_term: f32 = select(0.0, d.y, d.y == d.x);
  var delta: f32 = select(dy_term, d.y - min(d.y, (d.y - d.x) * s.y / s.x), abs(s.x) >= kMinNormalHalf);
  return delta * s.y + s.x * (1.0 - d.y) + d.x * (1.0 - s.y);
}
)";
    } break;
    case BlendMode::kSoftLight: {
      ss << R"(
fn soft_light_component(s: vec2<f32>, d: vec2<f32>) -> f32 {
    if 2.0 * s.x <= s.y {
      return d.x * d.x * (s.y - 2.0 * s.x) / d.y
          + (1.0 - d.y) * s.x
          + d.x * (-s.y + 2.0 * s.x + 1.0);
    } else if (4.0 * d.x <= d.y) {
      var DSqd: f32 = d.x * d.x;
      var DCub: f32 = DSqd * d.x;
      var DaSqd: f32 = d.y * d.y;
      var DaCub: f32 = DaSqd * d.y;
      return (DaSqd * (s.x - d.x * (3.0 * s.y - 6.0 * s.x - 1.0)) 
            + 12.0 * d.y * DSqd * (s.y - 2.0 * s.x)
            - 16.0 * DCub * (s.y - 2.0 * s.x)
            - DaCub * s.x) / DaSqd;
    } else {
        return d.x * (s.y - 2.0 * s.x + 1.0) + s.x - sqrt(d.y * d.x) * (s.y - 2.0 * s.x) - d.y * s.x;
    }
}
)";
      break;
    }
    case BlendMode::kHue:
    case BlendMode::kSaturation:
    case BlendMode::kColor:
    case BlendMode::kLuminosity: {
      ss << R"(
const kMinNormalHalf: f32 = 0.000001;

fn blend_color_luminance(color: vec3<f32>) -> f32 {
    return dot(vec3<f32>(0.3, 0.59, 0.11), color);
}

fn blend_set_color_luminance(hueSatColor: vec3<f32>, alpha: f32, lumColor: vec3<f32>) -> vec3<f32> {
    var lum: f32 = blend_color_luminance(lumColor);
    var result: vec3<f32> = vec3<f32>(lum - blend_color_luminance(hueSatColor)) + hueSatColor;
    var minComp: f32 = min(min(result.r, result.g), result.b);
    var maxComp: f32 = max(max(result.r, result.g), result.b);
    if (minComp < 0.0 && lum != minComp) {
        result = vec3<f32>(lum) + (result - vec3<f32>(lum)) * (lum / ((lum - minComp) + kMinNormalHalf));
    }
    if (maxComp > alpha && maxComp != lum) {
        result =
            vec3<f32>(lum) + (result - vec3<f32>(lum)) * (alpha - lum) / ((maxComp - lum) + kMinNormalHalf);
    }
    return result;
}

fn blend_color_saturation(color: vec3<f32>) -> f32 {
    return max(max(color.r, color.g), color.b) - min(min(color.r, color.g), color.b);
}

fn blend_set_color_saturation(color: vec3<f32>, satColor: vec3<f32>) -> vec3<f32> {
  var mn: f32 = min(min(color.r, color.g), color.b);
  var mx: f32 = max(max(color.r, color.g), color.b);
  if (mx > mn) {
    return ((color - vec3<f32>(mn)) * blend_color_saturation(satColor)) / (mx - mn);
  }

  return vec3<f32>(0.0, 0.0, 0.0);
}

fn blend_hslc(flipSat: vec2<f32>, src: vec4<f32>, dst: vec4<f32>) -> vec4<f32> {
  var alpha: f32 = dst.a * src.a;
  var sda: vec3<f32> = src.rgb * dst.a;
  var dsa: vec3<f32> = dst.rgb * src.a;
  var l: vec3<f32>;
  var r: vec3<f32>;

  if (flipSat.x != 0.0) {
    l = dsa;
    r = sda;
  } else {
    l = sda;
    r = dsa;
  }

  if (flipSat.y != 0.0) {
    l = blend_set_color_saturation(l, r);
    r = dsa;
  }

  return vec4<f32>(
      blend_set_color_luminance(l, alpha, r) + dst.rgb - dsa + src.rgb - sda,
      src.a + dst.a - alpha
  );
}
)";
      break;
    }

    default:
      break;
  }
}
}  // namespace

std::string WGXProgrammableBlending::GenSourceWGSL() const {
  std::stringstream ss;
  AppendHelperFunction(blend_mode_, ss);

  ss << "fn blending(src: vec4<f32>, dst: vec4<f32>) -> vec4<f32> {\n";
  ss << "var result: vec4<f32>;\n";
  switch (blend_mode_) {
    case BlendMode::kModulate: {
      ss << "result = src * dst;\n";
      break;
    }
    case BlendMode::kScreen: {
      ss << "result = src + (vec4<f32>(1.0) - src) * dst;\n";
      break;
    }
    case BlendMode::kOverlay: {
      ss << R"(
  result = vec4<f32>(blend_overlay_component(src.ra, dst.ra),
                     blend_overlay_component(src.ga, dst.ga),
                     blend_overlay_component(src.ba, dst.ba),
                     src.a + (1.0 - src.a) * dst.a);

  var extra: vec4<f32> = dst * (1.0 - src.a) + src * (1.0 - dst.a);
  extra.a = 0.0;
  result += extra;
      )";
      break;
    }
    case BlendMode::kDarken: {
      ss << R"(
  result = src + (1.0 - src.a) * dst;
  var rgb: vec3<f32> = min(result.rgb, (1.0 - dst.a) * src.rgb + dst.rgb);
  result = vec4<f32>(rgb, result.a);
      )";
      break;
    }
    case BlendMode::kLighten: {
      ss << R"(
  result = src + (1.0 - src.a) * dst;
  var rgb: vec3<f32> = max(result.rgb, (1.0 - dst.a) * src.rgb + dst.rgb);
  result = vec4<f32>(rgb, result.a);
      )";
      break;
    }

    case BlendMode::kColorDodge: {
      ss << R"(
  result = vec4<f32>(blend_color_dodge_component(src.ra, dst.ra),
                     blend_color_dodge_component(src.ga, dst.ga),
                     blend_color_dodge_component(src.ba, dst.ba),
                     src.a + (1.0 - src.a) * dst.a);
      )";
      break;
    }
    case BlendMode::kColorBurn: {
      ss << R"(
  result = vec4<f32>(blend_color_burn_component(src.ra, dst.ra),
                     blend_color_burn_component(src.ga, dst.ga),
                     blend_color_burn_component(src.ba, dst.ba),
                     src.a + (1.0 - src.a) * dst.a);
      )";
      break;
    }
    case BlendMode::kHardLight: {
      ss << R"(
  result = vec4<f32>(blend_overlay_component(dst.ra, src.ra),
                     blend_overlay_component(dst.ga, src.ga),
                     blend_overlay_component(dst.ba, src.ba),
                     dst.a + (1.0 - dst.a) * src.a);

  var extra: vec4<f32> = dst * (1.0 - src.a) + src * (1.0 - dst.a);
  extra.a = 0.0;
  result += extra;
      )";
      break;
    }
    case BlendMode::kSoftLight: {
      ss << R"(
  if dst.a == 0.0 {
    result = src;
  } else {
    result = vec4<f32>(soft_light_component(src.ra, dst.ra),
                       soft_light_component(src.ga, dst.ga),
                       soft_light_component(src.ba, dst.ba),
                       src.a + (1.0 - src.a) * dst.a);
  }
)";
      break;
    }
    case BlendMode::kDifference: {
      ss << R"(
  result = vec4<f32>(src.rgb + dst.rgb - 2.0 * min(src.rgb * dst.a, dst.rgb * src.a),
                     src.a + (1 - src.a) * dst.a);
)";
      break;
    }
    case BlendMode::kExclusion: {
      ss << R"(
  result = vec4<f32>(dst.rgb + src.rgb - 2.0 * dst.rgb * src.rgb,
                     src.a + (1.0 - src.a) * dst.a);
)";
      break;
    }
    case BlendMode::kMultiply: {
      ss << R"(
  result = vec4<f32>(
      (1.0 - src.a) * dst.rgb + (1.0 - dst.a) * src.rgb + src.rgb * dst.rgb,
      src.a + (1.0 - src.a) * dst.a);
)";
      break;
    }
    case BlendMode::kHue: {
      ss << "result = blend_hslc(vec2<f32>(0.0, 1.0), src, dst);\n";
      break;
    }
    case BlendMode::kSaturation: {
      ss << "result = blend_hslc(vec2<f32>(1.0, 1.0), src, dst);\n";
      break;
    }
    case BlendMode::kColor: {
      ss << "result = blend_hslc(vec2<f32>(0.0, 0.0), src, dst);\n";
      break;
    }
    case BlendMode::kLuminosity: {
      ss << "result = blend_hslc(vec2<f32>(1.0, 0.0), src, dst);\n";
      break;
    }
    default:
      ss << "result = src;\n";
  }
  ss << "return result;\n";
  ss << "}\n";

  return ss.str();
}

}  // namespace skity
