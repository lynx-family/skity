// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_path_visitor.hpp"

#include <array>
#include <skity/geometry/matrix.hpp>

#include "src/geometry/conic.hpp"

namespace skity {

HWPathVisitor::HWPathVisitor(Paint const& paint, bool approx_curve,
                             const Matrix& matrix,
                             VectorCache<float>* vertex_vector_cache,
                             VectorCache<uint32_t>* index_vector_cache)
    : PathVisitor(approx_curve, matrix),
      HWGeometryRaster(paint, matrix, vertex_vector_cache, index_vector_cache) {
}

void HWPathVisitor::OnClose() {}

}  // namespace skity
