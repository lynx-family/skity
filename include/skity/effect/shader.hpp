// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_EFFECT_SHADER_HPP
#define INCLUDE_SKITY_EFFECT_SHADER_HPP

#include <array>
#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/graphic/tile_mode.hpp>
#include <skity/macros.hpp>
#include <vector>

namespace skity {

class Pixmap;

/**
 * Shaders specify the source color(s) for what is being drawn.
 * if a paint has no shader, then the paint's color is used. If the paint has a
 * shader, then the shader's color(s) are use instead.
 */
class SKITY_API Shader {
 public:
  Shader() = default;
  virtual ~Shader() = default;
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;

  virtual bool IsOpaque() const { return false; }

  void SetLocalMatrix(Matrix const& matrix) { local_matrix_ = matrix; }
  Matrix GetLocalMatrix() const { return local_matrix_; }

  enum GradientType {
    kNone,
    kColor,
    kLinear,
    kRadial,
    kSweep,
    kConical,
  };

  struct GradientInfo {
    int32_t color_count;
    std::vector<Vec4> colors;
    std::vector<float> color_offsets;
    std::array<Point, 2> point;
    std::array<float, 2> radius;
    Matrix local_matrix;
    TileMode tile_mode;
    /**
     * By default gradients will interpolate their colors in unpremul space
     *  and then premultiply each of the results. By setting this flag to 1, the
     *  gradients will premultiply their colors first, and then interpolate
     *  between them.
     *
     */
    int32_t gradientFlags;
  };

  virtual GradientType AsGradient(GradientInfo* info) const;

  virtual const std::shared_ptr<Image>* AsImage() const;

  virtual const SamplingOptions* GetSamplingOptions() const;

  /**
   * Returns a shader that generates a linear gradient between the two specified
   * points.
   *
   * @param pts     The start and end points for the gradient.
   * @param colors  The array[count] of colors, to be distributed bteween the
   *                two points
   * @param pos     May be null or array[count] of floats for the relative
   *                position of each corresponding color in the colors array.
   * @param count   Must be >= 2. The number of colors (and pos if not NULL)
   *                entries.
   * @param flag    if set to 1, the gradients will premultiply their colors
   *                first, and then interpolate between them
   * @return        Then gradient shader instance
   */
  static std::shared_ptr<Shader> MakeLinear(
      const Point pts[2], const Vec4 colors[], const float pos[], int count,
      TileMode tile_mode = TileMode::kClamp, int flag = 0);

  /**
   * Returns a shader that generates a radial gradient given the center and
   * radius.
   *
   * @param center  The center of the circle for this gradient
   * @param radius  Must be positive. The radius of the circle for this gradient
   * @param colors  The array[count] of colors, to be distributed between the
   *                center and edge of the circle
   * @param pos     The array[count] of floats, or NULL. For the relative
   *                position of each corresponding color in the colors array.
   * @param count   Must be >= 2. The number of colors
   * @param flag    if set to 1, the gradients will premultiply their colors
   *                first, and then interpolate between them
   * @return        The gradient shader instance
   */
  static std::shared_ptr<Shader> MakeRadial(
      Point const& center, float radius, const Vec4 colors[], const float pos[],
      int count, TileMode tile_mode = TileMode::kClamp, int flag = 0);

  /**
   *  Returns a shader that generates a conical gradient given two circles, or
   *  returns NULL if the inputs are invalid. The gradient interprets the
   *  two circles according to the following HTML spec.
   *  https://html.spec.whatwg.org/#dom-context-2d-createradialgradient
   */
  static std::shared_ptr<Shader> MakeTwoPointConical(
      Point const& start, float start_radius, Point const& end,
      float end_radius, const Vec4 colors[], const float pos[], int count,
      TileMode tile_mode = TileMode::kClamp, int flag = 0);

  /**
   * Returns a shader that generates a sweep gradient given the center and
   * angles.
   *
   * @param cx          The X coordinate of the center of the sweep
   * @param cy          The Y coordinate of the center of the sweep
   * @param start_angle Start of the angular range, corresponding to pos == 0.
   * @param end_angle   End of the angular range, corresponding to pos == 1.
   * @param colors      The array[count] of colors, to be distributed between
   *                    the center and edge of the circle
   * @param pos         The array[count] of floats, or NULL. For the relative
   *                    position of each corresponding color in the colors
   *                    array.
   * @param count       Must be >= 2. The number of colors
   * @param flag        if set to 1, the gradients will premultiply their colors
   *                    first, and then interpolate between them
   * @return            The gradient shader instance
   */
  static std::shared_ptr<Shader> MakeSweep(
      float cx, float cy, float start_angle, float end_angle,
      const Vec4 colors[], const float pos[], int count,
      TileMode tile_mode = TileMode::kClamp, int flag = 0);

  /**
   * Return a shader that fill color with pixel content
   */
  static std::shared_ptr<Shader> MakeShader(
      std::shared_ptr<Image> image,
      const SamplingOptions& sampling = SamplingOptions{},
      TileMode x_tile_mode = TileMode::kClamp,
      TileMode y_tile_mode = TileMode::kClamp,
      const Matrix& local_matrix = Matrix());

 private:
  Matrix local_matrix_ = Matrix(1.f);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_EFFECT_SHADER_HPP
