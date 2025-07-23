// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_HW_GEOMETRY_RASTER_HPP
#define SRC_RENDER_HW_HW_GEOMETRY_RASTER_HPP

#include <array>
#include <glm/glm.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <vector>

#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/utils/vector_cache.hpp"

namespace skity {

class HWGeometryRaster {
 public:
  HWGeometryRaster(Paint const& paint, const Matrix& matrix,
                   VectorCache<float>* vertex_vector_cache,
                   VectorCache<uint32_t>* index_vector_cache);
  virtual ~HWGeometryRaster();

  void FillTextRect(Vec4 const& bounds, Vec2 const& uv_lt, Vec2 const& uv_rb);

  uint32_t GetFrontCount() const { return front_count_; }

  uint32_t GetBackCount() const { return back_count_; }

  const Matrix& GetTransform() { return matrix_; }

  // this is only used in test code
  const std::vector<float>& GetRawVertexBuffer() const {
    return vertex_buffer_;
  }

  const std::vector<uint32_t>& GetRawIndexBuffer() const {
    return index_buffer_;
  }

 protected:
  uint32_t AppendLineVertex(Vec2 const& p);

  uint32_t AppendLineVertex(Vec2 const& p, float alpha);

  uint32_t AppendVertex(float x, float y, float alpha);

  uint32_t AppendVertex(float x, float y, float u, float v);

  void AppendRect(uint32_t a, uint32_t b, uint32_t c, uint32_t d);

  void AppendFrontTriangle(uint32_t a, uint32_t b, uint32_t c);
  void AppendBackTriangle(uint32_t a, uint32_t b, uint32_t c);

 private:
  Paint paint_ = {};

  uint32_t front_count_ = {};
  uint32_t back_count_ = {};

  VectorCache<float>* vertex_vector_cache_;
  VectorCache<uint32_t>* index_vector_cache_;
  std::vector<float>& vertex_buffer_;
  std::vector<uint32_t>& index_buffer_;

  Matrix matrix_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_HW_GEOMETRY_RASTER_HPP
