// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_GRADIENT_SHADER_HPP
#define SRC_EFFECT_GRADIENT_SHADER_HPP

#include <skity/effect/shader.hpp>
#include <skity/geometry/rect.hpp>

namespace skity {

class GradientShader : public Shader {
 public:
  explicit GradientShader(GradientType type) : Shader(), info_(), type_(type) {}
  ~GradientShader() override = default;

  GradientType AsGradient(GradientInfo* info) const override;

 protected:
  GradientInfo* GetGradientInfo() { return &info_; }
  GradientType GetGradientType() { return type_; }

 private:
  void CopyInfo(GradientInfo* info) const;

 private:
  GradientInfo info_;
  GradientType type_;
};

class LinearGradientShader : public GradientShader {
 public:
  LinearGradientShader(const Point pts[2], const Vec4 colors[],
                       const float pos[], int32_t count, TileMode tile_mode,
                       int flag);

  ~LinearGradientShader() override = default;
};

class RadialGradientShader : public GradientShader {
 public:
  RadialGradientShader(Point const& center, float radius, const Vec4 colors[],
                       const float pos[], int32_t count, TileMode tile_mode,
                       int flag);
  ~RadialGradientShader() override = default;
};

class TwoPointConicalGradientShader : public GradientShader {
 public:
  TwoPointConicalGradientShader(Point const& start, float start_radius,
                                Point const& end, float end_radius,
                                const Vec4 colors[], const float pos[],
                                int32_t count, TileMode tile_mode, int flag);
  ~TwoPointConicalGradientShader() override = default;
};

class SweepGradientShader : public GradientShader {
 public:
  SweepGradientShader(float cx, float cy, float bias, float scale,
                      const Vec4 colors[], const float pos[], int32_t count,
                      TileMode tile_mode, int flag);
  ~SweepGradientShader() override = default;
};
}  // namespace skity

#endif  // SRC_EFFECT_GRADIENT_SHADER_HPP
