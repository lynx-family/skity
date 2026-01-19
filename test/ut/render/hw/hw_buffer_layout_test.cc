// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <vector>

#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/hw_buffer_layout_map.hpp"

namespace skity {
bool operator==(const GPUVertexBufferLayout& a,
                const GPUVertexBufferLayout& b) {
  return a.array_stride == b.array_stride && a.step_mode == b.step_mode &&
         a.attributes == b.attributes;
}

bool operator==(const GPUVertexAttribute& a, const GPUVertexAttribute& b) {
  return a.format == b.format && a.offset == b.offset &&
         a.shader_location == b.shader_location;
}
}  // namespace skity

TEST(HWBufferLayoutMap, GetBufferLayout) {
  for (uint32_t i = skity::HWGeometryKeyType::kPath;
       i <= static_cast<uint32_t>(skity::HWGeometryKeyType::kLast); i++) {
    std::vector<skity::GPUVertexBufferLayout> expected_buffer_layout;
    std::vector<skity::GPUVertexBufferLayout> actual_buffer_layout =
        *skity::HWBufferLayoutMap::GetInstance().GetBufferLayout(
            static_cast<skity::HWGeometryKeyType::Value>(i));

    switch (static_cast<skity::HWGeometryKeyType::Value>(i)) {
      case skity::HWGeometryKeyType::kPath:
        expected_buffer_layout = skity::WGSLPathGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kPathAA:
        expected_buffer_layout = skity::WGSLPathAAGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kTessFill:
        expected_buffer_layout =
            skity::WGSLTessPathFillGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kTessStroke:
        expected_buffer_layout =
            skity::WGSLTessPathStrokeGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kColorText:
        expected_buffer_layout = skity::WGSLTextGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kGradientText:
        expected_buffer_layout = skity::WGSLTextGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kRRect:
        expected_buffer_layout = skity::WGSLRRectGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kClip:
        expected_buffer_layout = skity::WGSLPathGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
      case skity::HWGeometryKeyType::kFilter:
        expected_buffer_layout = skity::WGSLFilterGeometry::GetBufferLayout();
        EXPECT_EQ(expected_buffer_layout, actual_buffer_layout);
        break;
    }
  }
}
