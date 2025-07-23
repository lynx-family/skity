// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_RENDER_PASS_GL_HPP
#define SRC_GPU_GL_GPU_RENDER_PASS_GL_HPP

#include <functional>
#include <unordered_map>

#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPURenderPassGL : public GPURenderPass {
 public:
  GPURenderPassGL(const GPURenderPassDescriptor& desc, uint32_t target_fbo)
      : GPURenderPass(desc), target_fbo_(target_fbo) {}

  ~GPURenderPassGL() override = default;

  void EncodeCommands(std::optional<GPUViewport> viewport,
                      std::optional<GPUScissorRect> scissor) override;

  void BlitFramebuffer(uint32_t src_fbo, uint32_t dst_fbo, const Rect& src_rect,
                       const Rect& dst_rect, uint32_t target_width,
                       uint32_t target_height);

  void SetAfterCleanupAction(std::function<void()> action) {
    after_cleanup_action_ = std::move(action);
  }

  uint32_t GetTargetFBO() const { return target_fbo_; }

 private:
  void Clear();

  void SetScissorBox(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

  void SetColorWriteMask(bool enable);

  void UseProgram(uint32_t program);

  void SetStencilState(bool enable, const GPUStencilState& state, uint32_t ref);

  void SetBlendFunc(uint32_t src, uint32_t dst);

  void BindBuffer(uint32_t target, uint32_t buffer);

  void SetDepthState(bool enable, bool writable, GPUCompareFunction func);

 protected:
  uint32_t target_fbo_;
  bool need_free_fbo_ = true;
  bool enable_color_write_ = true;
  bool enable_stencil_test_ = false;
  uint32_t used_program_ = 0;
  uint32_t stencil_reference_ = 0;
  uint32_t blend_src_ = 0;
  uint32_t blend_dst_ = 0;
  bool disable_blend_ = false;
  GPUStencilState stencil_state_ = {};
  GPUScissorRect scissor_box_ = {};
  std::unordered_map<uint32_t, uint32_t> bound_buffer_ = {};
  std::function<void()> after_cleanup_action_ = nullptr;
};

class GLMSAAResolveRenderPass : public GPURenderPassGL {
 public:
  GLMSAAResolveRenderPass(const GPURenderPassDescriptor& desc,
                          uint32_t target_fbo, uint32_t resolve_fbo);

  ~GLMSAAResolveRenderPass() override = default;

  void EncodeCommands(std::optional<GPUViewport> viewport,
                      std::optional<GPUScissorRect> scissor) override;

 private:
  uint32_t resolve_fbo_;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_RENDER_PASS_GL_HPP
