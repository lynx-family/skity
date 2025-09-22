// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_WGX_UTILS_HPP
#define SRC_RENDER_HW_DRAW_WGX_UTILS_HPP

#include <wgsl_cross.h>

#include <skity/effect/shader.hpp>

#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_shader_function.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

struct Command;
struct HWDrawContext;

void UploadBindGroup(const wgx::BindGroupEntry* entry, Command* cmd,
                     HWDrawContext* ctx);

void UploadBindGroup(const wgx::BindGroupEntry* entry, Command* cmd,
                     const std::shared_ptr<GPUSampler>& sampler);

void UploadBindGroup(const wgx::BindGroupEntry* entry, Command* cmd,
                     const std::shared_ptr<GPUTexture>& texture);

GPUShaderStageMask ToShaderStage(wgx::ShaderStage stage);

const char* RemapTileFunction();

void ReplacePlaceholder(
    std::string& wgsl,
    const std::unordered_map<std::string, std::string>& replacements);

/**
 * Common code generator for all vertex shader.
 *
 * It contains the struct for common vertex info:
 * ```
 *  struct CommonInfo {
 *    mvp             : mat4x4<f32>,
 *    userTransform   : mat4x4<f32>,
 *    extraInfo       : mat4x4<f32>,
 *  };
 * ```
 *
 * It also contains the function to calculate the final position:
 *
 * fn get_vertex_position(a_pos: vec2<f32>, cs: CommonSlot) -> vec4<f32>;
 */
const char* CommonVertexWGSL();

bool SetupCommonInfo(const wgx::BindGroupEntry* entry, const Matrix& mvp,
                     const Matrix& user_transform, float clip_depth);

bool SetupInvMatrix(const wgx::BindGroupEntry* entry,
                    const Matrix& local_matrix);

bool SetupImageBoundsInfo(const wgx::BindGroupEntry* image_bounds_entry,
                          const Matrix& local_matrix, float width,
                          float height);

/**
 * Common code generator for Gradient Shader.
 * It contains the struct for common gradient info:
 * ```
 *  struct GradientInfo {
 *    infos        : vec4<i32>,
 *    colors       : array<vec4<f32>, N>,
 *    stops        : array<vec4<f32>, N / 4>,
 *    global_alpha : f32,
 *  };
 * ```
 *
 * ConicalInfo if gradient type is conical:
 * ```
 *  struct ConicalInfo {
 *    center1  : vec2<f32>,
 *    center2  : vec2<f32>,
 *    radius1  : f32,
 *    radius2  : f32,
 *  };
 * ```
 *
 *
 * Also contains common function to calculate the gradient value:
 *
 * fn calculate_gradient_color(t: f32, info: GradientInfo) -> vec4<f32>;
 *
 * and the function to calculate the gradient t value for conical graident:
 *
 * fn compute_conical_t(pos: vec2<f32>,
 *                      c0 : vec2<f32>,
 *                      r0 : f32,
 *                      c1 : vec2<f32>,
 *                      r1 : f32) -> vec2<f32>;
 *
 */
class WGXGradientFragment {
 public:
  WGXGradientFragment(const Shader::GradientInfo& info,
                      Shader::GradientType type);

  std::string GenSourceWGSL(size_t index) const;

  std::string GetShaderName() const;

  bool SetupCommonInfo(const wgx::BindGroupEntry* info_entry,
                       float global_alpha) const;

  bool SetupGradientInfo(const wgx::BindGroupEntry* info_entry) const;

 private:
  const char* GradientTypeName() const;

  uint32_t GetOffsetCount() const;

  uint32_t RoundGradientColorCount() const;

  std::string GenerateGradientCommonWGSL(size_t index) const;

  bool SetupLinearInfo(const wgx::BindGroupEntry* info_entry) const;

  bool SetupRadialInfo(const wgx::BindGroupEntry* info_entry) const;

  bool SetupConicalInfo(const wgx::BindGroupEntry* info_entry) const;

  bool SetupSweepInfo(const wgx::BindGroupEntry* info_entry) const;

 private:
  const Shader::GradientInfo& info_;
  Shader::GradientType type_;
  uint32_t max_color_count_ = 0;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_WGX_UTILS_HPP
