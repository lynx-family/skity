// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_SURFACE_GL_HPP
#define SRC_GPU_GL_GPU_SURFACE_GL_HPP

#include <skity/gpu/gpu_context_gl.hpp>

#include "src/gpu/gpu_surface_impl.hpp"

namespace skity {

class GPUSurfaceGL : public GPUSurfaceImpl {
 public:
  GPUSurfaceGL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx);

  ~GPUSurfaceGL() override;

  virtual void Init();

  std::shared_ptr<Pixmap> ReadPixels(const Rect& rect) override;

  GPUTextureFormat GetGPUFormat() const override {
    return GPUTextureFormat::kRGBA8Unorm;
  }

 protected:
  uint32_t GetVertexArray() const { return vao_; }

 private:
  uint32_t vao_ = 0;
};

class DirectSurfaceGL : public GPUSurfaceGL {
 public:
  DirectSurfaceGL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                  uint32_t fbo_id, bool need_free);

  ~DirectSurfaceGL() override;

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 private:
  uint32_t target_fbo_id_;
  bool need_free_fbo_;
};

class TextureSurfaceGL : public GPUSurfaceGL {
 public:
  TextureSurfaceGL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                   std::shared_ptr<GPUTexture> texture);

  ~TextureSurfaceGL() override = default;

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 private:
  std::shared_ptr<GPUTexture> ext_texture_;
};

class DrawTextureSurfaceGL : public GPUSurfaceGL {
 public:
  DrawTextureSurfaceGL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                       std::shared_ptr<GPUTexture> texture,
                       uint32_t resolve_fbo, bool can_blit_from_target_fbo)
      : GPUSurfaceGL(desc, ctx),
        color_attachment_(std::move(texture)),
        resolve_fbo_(resolve_fbo),
        can_blit_from_target_fbo_(can_blit_from_target_fbo) {}

  ~DrawTextureSurfaceGL() override = default;

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override {}

 protected:
  std::shared_ptr<GPUTexture> color_attachment_;
  uint32_t resolve_fbo_;
  bool can_blit_from_target_fbo_;
};

class PartialFBOSurfaceGL : public DrawTextureSurfaceGL {
 public:
  PartialFBOSurfaceGL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                      std::shared_ptr<GPUTexture> texture, uint32_t resolve_fbo)
      : DrawTextureSurfaceGL(desc, ctx, std::move(texture), resolve_fbo,
                             false) {}

  ~PartialFBOSurfaceGL() override = default;

  void SetFrameInfo(const PartialFrameInfo& info) { frame_info_ = info; }

  void UpdateTranslate(float dx, float dy) {
    translate_x_ = dx;
    translate_y_ = dy;
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

 private:
  PartialFrameInfo frame_info_ = {};
  float translate_x_ = 0.f;
  float translate_y_ = 0.f;
};

class BlitSurfaceGL : public GPUSurfaceGL {
 public:
  BlitSurfaceGL(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                uint32_t resolve_fbo, bool can_blit_from_target_fbo);

  ~BlitSurfaceGL() override = default;

  void Init() override;

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override;

  void OnFlush() override;

 private:
  uint32_t resolve_fbo_;
  std::shared_ptr<GPUTexture> texture_ = {};
  bool can_blit_from_target_fbo_;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_SURFACE_GL_HPP
