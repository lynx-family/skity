// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_PATH_VISITOR_HPP
#define SRC_RENDER_HW_HW_PATH_VISITOR_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>

#include "src/graphic/path_visitor.hpp"
#include "src/render/hw/hw_geometry_raster.hpp"

namespace skity {

class HWPathVisitor : public PathVisitor, public HWGeometryRaster {
 public:
  HWPathVisitor(Paint const& paint, bool approx_curve, const Matrix& matrix,
                VectorCache<float>* vertex_vector_cache,
                VectorCache<uint32_t>* index_vector_cache);
  ~HWPathVisitor() override = default;

 protected:
  void OnClose() override;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_PATH_VISITOR_HPP
