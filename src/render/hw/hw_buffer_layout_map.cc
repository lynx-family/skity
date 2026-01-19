// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_buffer_layout_map.hpp"

#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_fill_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_tess_path_stroke_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

HWBufferLayoutMap& HWBufferLayoutMap::GetInstance() {
  static NoDestructor<HWBufferLayoutMap> instance;
  return *instance;
}

void HWBufferLayoutMap::RegisterAllBufferLayouts() {
  buffer_layout_map_[HWGeometryKeyType::kPath] =
      std::move(WGSLPathGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kPathAA] =
      std::move(WGSLPathAAGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kTessFill] =
      std::move(WGSLTessPathFillGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kTessStroke] =
      std::move(WGSLTessPathStrokeGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kColorText] =
      std::move(WGSLTextGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kGradientText] =
      std::move(WGSLTextGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kRRect] =
      std::move(WGSLRRectGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kClip] =
      std::move(WGSLPathGeometry::GetBufferLayout());
  buffer_layout_map_[HWGeometryKeyType::kFilter] =
      std::move(WGSLFilterGeometry::GetBufferLayout());
}

HWBufferLayoutMap::HWBufferLayoutMap() { RegisterAllBufferLayouts(); }

}  // namespace skity
