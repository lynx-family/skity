// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SHADER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SHADER_HPP

// Shader wrapper of the header-only RAII layer: gradient and image-fill
// factories over the C entries (linear / radial / sweep / two-point conical
// / image). See skity.hpp for the layer's design.

#include <skity_c/skity_shader.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_image.hpp>
#include <skity_hpp/skity_types.hpp>
#include <vector>

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

  /** Sweep (angular) gradient from @p start_angle to @p end_angle, both in
   *  degrees around (@p cx, @p cy). */
  static Shader MakeSweep(float cx, float cy, float start_angle,
                          float end_angle, const Color4f* colors,
                          const float* pos, int32_t count,
                          TileMode mode = TileMode::kClamp, int32_t flags = 0) {
    return Shader(skity_shader_create_sweep(
        cx, cy, start_angle, end_angle, colors, pos, count, to_c(mode), flags));
  }

  /**
   * Two-point conical gradient (radial between two circles), following the
   * HTML canvas createRadialGradient spec.
   */
  static Shader MakeTwoPointConical(Point start, float start_radius, Point end,
                                    float end_radius, const Color4f* colors,
                                    const float* pos, int32_t count,
                                    TileMode mode = TileMode::kClamp,
                                    int32_t flags = 0) {
    return Shader(skity_shader_create_two_point_conical(
        start, start_radius, end, end_radius, colors, pos, count, to_c(mode),
        flags));
  }

  /**
   * Image-fill shader (legacy MakeShader shape): tiles @p image's pixels.
   * An empty image yields an empty wrapper.
   */
  static Shader MakeShader(const Image& image,
                           const SamplingOptions& sampling = SamplingOptions(),
                           TileMode x_tile_mode = TileMode::kClamp,
                           TileMode y_tile_mode = TileMode::kClamp,
                           const Matrix& local_matrix = Matrix()) {
    return Shader(skity_shader_create_image(image.get(), &sampling,
                                            to_c(x_tile_mode),
                                            to_c(y_tile_mode), &local_matrix));
  }

  /**
   * Adopt an owning C handle (e.g. from skity_paint_get_shader); destroying
   * the wrapper releases the shared reference.
   */
  static Shader Adopt(skity_shader h) { return Shader(h); }

  /** Transform applied when the shader is used (legacy naming). */
  void SetLocalMatrix(const Matrix& matrix) {
    skity_shader_set_local_matrix(get(), &matrix);
  }

  Matrix GetLocalMatrix() const {
    Matrix out;
    skity_shader_get_local_matrix(get(), &out);
    return out;
  }

  /** Whether the shader is guaranteed opaque. */
  bool IsOpaque() const { return skity_shader_is_opaque(get()) != 0u; }

  /** Gradient classification, mirroring the legacy GradientType. */
  enum class GradientType : uint32_t {
    kNone = SKITY_GRADIENT_TYPE_NONE,
    kColor = SKITY_GRADIENT_TYPE_COLOR,
    kLinear = SKITY_GRADIENT_TYPE_LINEAR,
    kRadial = SKITY_GRADIENT_TYPE_RADIAL,
    kSweep = SKITY_GRADIENT_TYPE_SWEEP,
    kConical = SKITY_GRADIENT_TYPE_CONICAL,
  };

  /** Gradient geometry passthrough (color stops fetched separately). */
  using GradientInfo = skity_gradient_info;

  /**
   * Classify the shader and recover gradient parameters. On return @p info
   * carries the stop count / geometry; when @p colors / @p offsets are
   * non-NULL they are resized to the stop count and filled.
   */
  GradientType AsGradient(GradientInfo* info, std::vector<Color4f>* colors,
                          std::vector<float>* offsets) const {
    GradientInfo tmp{};
    auto raw = skity_shader_as_gradient(get(), nullptr, nullptr, 0,
                                        info != nullptr ? info : &tmp);
    if (colors != nullptr && offsets != nullptr &&
        (info != nullptr ? info->color_count : tmp.color_count) > 0) {
      int32_t n = info != nullptr ? info->color_count : tmp.color_count;
      colors->resize((size_t)n);
      offsets->resize((size_t)n);
      skity_shader_as_gradient(get(), colors->data(), offsets->data(), n,
                               nullptr);
    }
    return static_cast<GradientType>(raw);
  }

 private:
  explicit Shader(skity_shader h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SHADER_HPP
