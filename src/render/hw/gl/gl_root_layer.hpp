// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_GL_GL_ROOT_LAYER_HPP
#define SRC_RENDER_HW_GL_GL_ROOT_LAYER_HPP

#include "src/gpu/gl/gl_interface.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity {

class GPUBufferGL;

class GLRootLayer : public HWRootLayer {
 public:
  GLRootLayer(uint32_t width, uint32_t height, const Rect &bounds, GLuint vao);

  ~GLRootLayer() override = default;

 protected:
  void Draw(GPURenderPass *render_pass) override;

  void OnPostDraw(GPURenderPass *render_pass, GPUCommandBuffer *cmd) override;

 private:
  void BindVAO();

  void UnBindVAO();

 private:
  GLuint vao_;
};

class GLDirectRootLayer : public GLRootLayer {
 public:
  GLDirectRootLayer(uint32_t width, uint32_t height, const Rect &bounds,
                    GLuint vao, GLuint fbo);

 protected:
  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer *cmd) override;

 private:
  GLuint fbo_id_;
};

class GLExternTextureLayer : public GLRootLayer {
 public:
  GLExternTextureLayer(std::shared_ptr<GPUTexture> texture, const Rect &bounds,
                       GLuint vao, int32_t src_fbo = -1);

  ~GLExternTextureLayer() override = default;

 protected:
  HWDrawState OnPrepare(HWDrawContext *context) override;

  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer *cmd) override;

 private:
  std::shared_ptr<GPUTexture> ext_texture_;
  GPURenderPassDescriptor render_pass_desc_ = {};
  int32_t src_fbo_;
};

class GLDrawTextureLayer : public GLRootLayer {
 public:
  GLDrawTextureLayer(std::shared_ptr<GPUTexture> texture, GLuint resolve_fbo,
                     const Rect &bounds, GLuint vao,
                     bool can_blit_from_target_fbo);

  ~GLDrawTextureLayer() override = default;

 protected:
  HWDrawState OnPrepare(HWDrawContext *context) override;

  void OnGenerateCommand(HWDrawContext *context, HWDrawState state) override;

  std::shared_ptr<GPURenderPass> OnBeginRenderPass(
      GPUCommandBuffer *cmd) override;

  void OnPostDraw(GPURenderPass *render_pass, GPUCommandBuffer *cmd) override;

 protected:
  std::shared_ptr<GPUTexture> color_texture_;
  GLuint resolve_fbo_;
  GPURenderPassDescriptor render_pass_desc_ = {};
  HWDraw *layer_back_draw_ = {};
  bool can_blit_from_target_fbo_;
};

class GLPartialDrawTextureLayer : public GLDrawTextureLayer {
 public:
  GLPartialDrawTextureLayer(std::shared_ptr<GPUTexture> texture,
                            GLuint resolve_fbo, const Rect &bounds, GLuint vao);

  ~GLPartialDrawTextureLayer() override = default;

  void SetFrameInfo(uint32_t width, uint32_t height, int32_t left, int32_t top,
                    int32_t right, int32_t bottom) {
    target_width_ = width;
    target_height_ = height;

    left_ = left;
    top_ = top;
    right_ = right;
    bottom_ = bottom;

    SetScissorBox(Rect::MakeLTRB(left_, top_, right_, bottom_));
  }

  void UpdateTranslate(float dx, float dy) {
    dx_ = dx;
    dy_ = dy;
  }

 protected:
  HWDrawState OnPrepare(skity::HWDrawContext *context) override;

  void OnPostDraw(skity::GPURenderPass *render_pass,
                  skity::GPUCommandBuffer *cmd) override;

 private:
  uint32_t target_width_ = 0;
  uint32_t target_height_ = 0;
  int32_t left_ = 0;
  int32_t top_ = 0;
  int32_t right_ = 0;
  int32_t bottom_ = 0;

  float dx_ = 0.f;
  float dy_ = 0.f;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_GL_GL_ROOT_LAYER_HPP
