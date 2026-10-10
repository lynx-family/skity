// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_shader.h>

#include <skity/effect/shader.hpp>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/point.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/sampling_options.hpp>

#include "handle.hpp"

namespace {

skity::Shader* shader_of(skity_shader handle) {
  auto* w =
      skity::capi::resolve<skity_shader_s>(handle, SKITY_OBJECT_TYPE_SHADER);
  return w ? static_cast<skity::Shader*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_shader skity_shader_create_linear(const skity_point pts[2],
                                        const skity_color4f* colors,
                                        const float* pos, int32_t count,
                                        skity_tile_mode tile_mode,
                                        int32_t flags) {
  auto sp = skity::Shader::MakeLinear(
      reinterpret_cast<const skity::Point*>(pts),
      reinterpret_cast<const skity::Vec4*>(colors), pos, count,
      static_cast<skity::TileMode>(tile_mode), flags);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_shader_s>(
      SKITY_OBJECT_TYPE_SHADER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_shader skity_shader_create_radial(skity_point center, float radius,
                                        const skity_color4f* colors,
                                        const float* pos, int32_t count,
                                        skity_tile_mode tile_mode,
                                        int32_t flags) {
  auto sp = skity::Shader::MakeRadial(
      *reinterpret_cast<const skity::Point*>(&center), radius,
      reinterpret_cast<const skity::Vec4*>(colors), pos, count,
      static_cast<skity::TileMode>(tile_mode), flags);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_shader_s>(
      SKITY_OBJECT_TYPE_SHADER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_shader skity_shader_create_sweep(float cx, float cy, float start_angle,
                                       float end_angle,
                                       const skity_color4f* colors,
                                       const float* pos, int32_t count,
                                       skity_tile_mode tile_mode,
                                       int32_t flags) {
  auto sp = skity::Shader::MakeSweep(
      cx, cy, start_angle, end_angle,
      reinterpret_cast<const skity::Vec4*>(colors), pos, count,
      static_cast<skity::TileMode>(tile_mode), flags);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_shader_s>(
      SKITY_OBJECT_TYPE_SHADER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_shader skity_shader_create_two_point_conical(
    skity_point start, float start_radius, skity_point end, float end_radius,
    const skity_color4f* colors, const float* pos, int32_t count,
    skity_tile_mode tile_mode, int32_t flags) {
  auto sp = skity::Shader::MakeTwoPointConical(
      *reinterpret_cast<const skity::Point*>(&start), start_radius,
      *reinterpret_cast<const skity::Point*>(&end), end_radius,
      reinterpret_cast<const skity::Vec4*>(colors), pos, count,
      static_cast<skity::TileMode>(tile_mode), flags);
  if (sp == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_shader_s>(
      SKITY_OBJECT_TYPE_SHADER, SKITY_HANDLE_OWNING, std::move(sp));
}

skity_shader skity_shader_create_image(skity_image image,
                                       const skity_sampling_options* sampling,
                                       skity_tile_mode x_tile_mode,
                                       skity_tile_mode y_tile_mode,
                                       const skity_matrix* local_matrix) {
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  if (img == nullptr) {
    return nullptr;
  }
  skity::SamplingOptions so{};
  if (sampling != nullptr) {
    so.filter = static_cast<skity::FilterMode>(sampling->filter);
    so.mipmap = static_cast<skity::MipmapMode>(sampling->mipmap);
    so.cubic.B = sampling->cubic_b;
    so.cubic.C = sampling->cubic_c;
  }
  skity::Matrix lm = local_matrix != nullptr
                         ? *reinterpret_cast<const skity::Matrix*>(local_matrix)
                         : skity::Matrix{};
  auto sp = skity::Shader::MakeShader(
      img, so, static_cast<skity::TileMode>(x_tile_mode),
      static_cast<skity::TileMode>(y_tile_mode), lm);
  if (sp == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_shader_s>(
      SKITY_OBJECT_TYPE_SHADER, SKITY_HANDLE_OWNING, std::move(sp));
}

void skity_shader_set_local_matrix(skity_shader shader,
                                   const skity_matrix* matrix) {
  auto* s = shader_of(shader);
  if (s == nullptr) return;
  // A NULL matrix resets the local matrix to identity, matching the header
  // documentation. SetLocalMatrix replaces (rather than composes with) the
  // stored matrix, so passing an identity matrix fully clears it.
  skity::Matrix m = matrix != nullptr
                        ? *reinterpret_cast<const skity::Matrix*>(matrix)
                        : skity::Matrix{};
  s->SetLocalMatrix(m);
}

void skity_shader_get_local_matrix(skity_shader shader, skity_matrix* out) {
  auto* s = shader_of(shader);
  if (s == nullptr || out == nullptr) return;
  *reinterpret_cast<skity::Matrix*>(out) = s->GetLocalMatrix();
}

void skity_shader_destroy(skity_shader shader) {
  skity::capi::destroy_handle<skity_shader_s>(shader, SKITY_OBJECT_TYPE_SHADER);
}

uint32_t skity_shader_is_opaque(skity_shader shader) {
  auto* s = shader_of(shader);
  return s && s->IsOpaque() ? 1u : 0u;
}

skity_gradient_type skity_shader_as_gradient(skity_shader shader,
                                             skity_color4f* colors,
                                             float* color_offsets,
                                             int32_t capacity,
                                             skity_gradient_info* info) {
  auto* s = shader_of(shader);
  if (s == nullptr) {
    return SKITY_GRADIENT_TYPE_NONE;
  }
  skity::Shader::GradientInfo gi{};
  auto type = s->AsGradient(&gi);
  if (info != nullptr) {
    info->color_count = gi.color_count;
    for (size_t i = 0; i < 2; i++) {
      info->point[i] = *reinterpret_cast<const skity_point*>(&gi.point[i]);
      info->radius[i] = gi.radius[i];
    }
    info->local_matrix =
        *reinterpret_cast<const skity_matrix*>(&gi.local_matrix);
    info->tile_mode = static_cast<skity_tile_mode>(gi.tile_mode);
    info->gradient_flags = gi.gradientFlags;
  }
  if (colors != nullptr && color_offsets != nullptr && capacity > 0) {
    int32_t n = gi.color_count < capacity ? gi.color_count : capacity;
    // Gradients created without explicit stops (NULL pos) carry an empty
    // color_offsets array; report the implied even distribution instead of
    // indexing into the empty vector.
    bool even = gi.color_offsets.empty();
    for (int32_t i = 0; i < n; i++) {
      colors[i] = *reinterpret_cast<const skity_color4f*>(&gi.colors[i]);
      color_offsets[i] = even && n > 1 ? (float)i / (float)(n - 1)
                                       : (even ? 0.f : gi.color_offsets[i]);
    }
  }
  return static_cast<skity_gradient_type>(type);
}

}  // extern "C"
