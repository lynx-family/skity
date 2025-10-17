// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/effect/gradient_shader.hpp"

#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

namespace skity {

namespace {
void EnsureColorOffsetsNonDescending(std::vector<float> &color_offsets,
                                     int32_t count) {
  for (int i = 0; i < count - 1; i++) {
    if (color_offsets[i] > color_offsets[i + 1]) {
      color_offsets[i + 1] = color_offsets[i];
    }
  }
}
}  // namespace

GradientShader::GradientType GradientShader::AsGradient(
    GradientInfo *info) const {
  if (info) {
    CopyInfo(info);
  }
  return type_;
}

void GradientShader::FlattenToBuffer(WriteBuffer &buffer) const {
  uint32_t flags = 0;

  if (!info_.color_offsets.empty()) {
    flags |= kHasPosition_GSF;
  }

  // we do not have color space

  // premul alpha
  if (info_.gradientFlags != 0) {
    flags |= kInterpolationInPremul_GSF;
  }

  flags |= (static_cast<uint32_t>(info_.tile_mode) << kTileModeShift_GSF);
  // skip the interpolation color space since it always be zero
  // skip the interpolation hue method since it always be zero

  buffer.WriteUint32(flags);

  int32_t color_count = info_.color_count;
  const auto colors = info_.colors.data();
  const auto positions =
      info_.color_offsets.empty() ? nullptr : info_.color_offsets.data();

  buffer.WriteColor4fArray(colors, color_count);

  if (positions) {
    buffer.WriteFloatArray(positions, color_count);
  }
}

void GradientShader::CopyInfo(GradientInfo *info) const {
  info->color_count = info_.color_count;
  std::memcpy(info->point.data(), info_.point.data(),
              info_.point.size() * sizeof(Point));

  std::memcpy(info->radius.data(), info_.radius.data(),
              info_.radius.size() * sizeof(float));

  info->colors.resize(info_.color_count);
  std::memcpy(info->colors.data(), info_.colors.data(),
              info_.color_count * sizeof(Vec4));

  if (!info_.color_offsets.empty()) {
    info->color_offsets.resize(info_.color_offsets.size());
    std::memcpy(info->color_offsets.data(), info_.color_offsets.data(),
                info_.color_offsets.size() * sizeof(float));
  }

  info->local_matrix = GetLocalMatrix();
  info->tile_mode = info_.tile_mode;
  info->gradientFlags = info_.gradientFlags;
}

LinearGradientShader::LinearGradientShader(const Point *pts, const Vec4 *colors,
                                           const float *pos, int32_t count,
                                           TileMode tile_mode, int flag)
    : GradientShader(GradientType::kLinear) {
  GradientInfo *info = GetGradientInfo();

  // points
  info->point[0] = pts[0];
  info->point[1] = pts[1];
  // colors
  info->color_count = count;
  info->colors.resize(count);
  std::memcpy(info->colors.data(), colors, count * sizeof(Vec4));
  // pos
  if (pos) {
    info->color_offsets.resize(count);
    std::memcpy(info->color_offsets.data(), pos, count * sizeof(float));
    EnsureColorOffsetsNonDescending(info->color_offsets, count);
  }
  info->local_matrix = Matrix(1.f);
  info->tile_mode = tile_mode;
  info->gradientFlags = flag;
}

std::string_view LinearGradientShader::ProcName() const {
  return "SkLinearGradient";
}

void LinearGradientShader::FlattenToBuffer(WriteBuffer &buffer) const {
  GradientShader::FlattenToBuffer(buffer);

  GradientInfo info{};

  AsGradient(&info);

  buffer.WritePoint(Vec2{info.point[0].x, info.point[0].y});
  buffer.WritePoint(Vec2{info.point[1].x, info.point[1].y});
}

RadialGradientShader::RadialGradientShader(Point const &center, float radius,
                                           const Vec4 colors[],
                                           const float pos[], int32_t count,
                                           TileMode tile_mode, int flag)
    : GradientShader(GradientType::kRadial) {
  GradientInfo *info = GetGradientInfo();
  // points
  info->point[0] = center;
  // radius
  info->radius[0] = radius;
  // colors
  info->color_count = count;
  info->colors.resize(count);
  std::memcpy(info->colors.data(), colors, count * sizeof(Vec4));
  // pos
  if (pos) {
    info->color_offsets.resize(count);
    std::memcpy(info->color_offsets.data(), pos, count * sizeof(float));
    EnsureColorOffsetsNonDescending(info->color_offsets, count);
  }

  info->local_matrix = Matrix(1.f);
  info->tile_mode = tile_mode;
  info->gradientFlags = flag;
}

std::string_view RadialGradientShader::ProcName() const {
  return "SkRadialGradient";
}

void RadialGradientShader::FlattenToBuffer(WriteBuffer &buffer) const {
  GradientShader::FlattenToBuffer(buffer);

  GradientInfo info{};

  AsGradient(&info);

  buffer.WritePoint(Vec2{info.point[0].x, info.point[0].y});
  buffer.WriteFloat(info.radius[0]);
}

TwoPointConicalGradientShader::TwoPointConicalGradientShader(
    Point const &start, float start_radius, Point const &end, float end_radius,
    const Vec4 colors[], const float pos[], int32_t count, TileMode tile_mode,
    int flag)
    : GradientShader(GradientType::kConical) {
  GradientInfo *info = GetGradientInfo();
  // points
  info->point[0] = start;
  info->point[1] = end;
  // radius
  info->radius[0] = start_radius;
  info->radius[1] = end_radius;
  // colors
  info->color_count = count;
  info->colors.resize(count);
  std::memcpy(info->colors.data(), colors, count * sizeof(Vec4));
  // pos
  if (pos) {
    info->color_offsets.resize(count);
    std::memcpy(info->color_offsets.data(), pos, count * sizeof(float));
    EnsureColorOffsetsNonDescending(info->color_offsets, count);
  }

  info->local_matrix = Matrix(1.f);
  info->tile_mode = tile_mode;
  info->gradientFlags = flag;
}

std::string_view TwoPointConicalGradientShader::ProcName() const {
  return "SkConicalGradient";
}

void TwoPointConicalGradientShader::FlattenToBuffer(WriteBuffer &buffer) const {
  GradientShader::FlattenToBuffer(buffer);

  GradientInfo info{};

  AsGradient(&info);

  buffer.WritePoint(Vec2{info.point[0].x, info.point[0].y});
  buffer.WritePoint(Vec2{info.point[1].x, info.point[1].y});

  buffer.WriteFloat(info.radius[0]);
  buffer.WriteFloat(info.radius[1]);
}

SweepGradientShader::SweepGradientShader(float cx, float cy, float bias,
                                         float scale, const Vec4 colors[],
                                         const float pos[], int32_t count,
                                         TileMode tile_mode, int flag)
    : GradientShader(GradientType::kSweep) {
  GradientInfo *info = GetGradientInfo();
  // points
  info->point[0].x = cx;
  info->point[0].y = cy;
  // radius
  info->radius[0] = bias;
  info->radius[1] = scale;
  // colors
  info->color_count = count;
  info->colors.resize(count);
  std::memcpy(info->colors.data(), colors, count * sizeof(Vec4));
  // pos
  if (pos) {
    info->color_offsets.resize(count);
    std::memcpy(info->color_offsets.data(), pos, count * sizeof(float));
    EnsureColorOffsetsNonDescending(info->color_offsets, count);
  }

  info->local_matrix = Matrix(1.f);
  info->tile_mode = tile_mode;
  info->gradientFlags = flag;
}

std::string_view SweepGradientShader::ProcName() const {
  return "SkSweepGradient";
}

void SweepGradientShader::FlattenToBuffer(WriteBuffer &buffer) const {
  GradientShader::FlattenToBuffer(buffer);

  GradientInfo info{};

  AsGradient(&info);

  buffer.WritePoint(Vec2{info.point[0].x, info.point[0].y});

  buffer.WriteFloat(info.radius[0]);  // bias
  buffer.WriteFloat(info.radius[1]);  // scale
}

}  // namespace skity
