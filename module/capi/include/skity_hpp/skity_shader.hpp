// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SHADER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SHADER_HPP

// Shader wrapper of the header-only RAII layer: gradient factories over the
// C entries (linear / radial to start; sweep, conical and image shaders
// join when a proven caller needs them). See skity.hpp for the layer's
// design.

#include <skity_c/skity_shader.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Gradient shader wrapper. Attach with Paint::SetShader. */
class Shader : public detail::OwnHandle<skity_shader, skity_shader_destroy> {
 public:
  /**
   * Linear gradient between two points. @p pts uses only x/y of each Vec4;
   * @p pos may be NULL for evenly distributed stops.
   */
  static Shader MakeLinear(const Point pts[2], const Color4f* colors,
                           const float* pos, int32_t count,
                           TileMode mode = TileMode::kClamp,
                           int32_t flags = 0) {
    return Shader(
        skity_shader_create_linear(pts, colors, pos, count, to_c(mode), flags));
  }

  /** Radial gradient from @p center (x/y used). */
  static Shader MakeRadial(Point center, float radius, const Color4f* colors,
                           const float* pos, int32_t count,
                           TileMode mode = TileMode::kClamp,
                           int32_t flags = 0) {
    return Shader(skity_shader_create_radial(center, radius, colors, pos, count,
                                             to_c(mode), flags));
  }

  /** Transform applied when the shader is used (legacy naming). */
  void SetLocalMatrix(const Matrix& matrix) {
    skity_shader_set_local_matrix(get(), &matrix);
  }

  Matrix GetLocalMatrix() const {
    Matrix out;
    skity_shader_get_local_matrix(get(), &out);
    return out;
  }

 private:
  explicit Shader(skity_shader h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SHADER_HPP
