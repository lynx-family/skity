// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_TEXTURE_GL_HPP
#define SRC_GPU_GL_GPU_TEXTURE_GL_HPP

#include <optional>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_texture.hpp"

namespace skity {

class GPUSamplerGL;

struct GLFramebufferHolder {
  uint32_t fbo_id = 0;
  bool need_free = false;

  GLFramebufferHolder() = default;
  GLFramebufferHolder(uint32_t fbo_id, bool need_free)
      : fbo_id(fbo_id), need_free(need_free) {}

  GLFramebufferHolder(const GLFramebufferHolder& other) = delete;
  GLFramebufferHolder& operator=(const GLFramebufferHolder& other) = delete;

  GLFramebufferHolder(GLFramebufferHolder&& other) {
    fbo_id = other.fbo_id;
    need_free = other.need_free;
    other.fbo_id = 0;
    other.need_free = false;
  }

  GLFramebufferHolder& operator=(GLFramebufferHolder&& other) {
    fbo_id = other.fbo_id;
    need_free = other.need_free;
    other.fbo_id = 0;
    other.need_free = false;
    return *this;
  }

  ~GLFramebufferHolder();
};

class GPUTextureGL : public GPUTexture {
 public:
  explicit GPUTextureGL(const GPUTextureDescriptor& descriptor);
  ~GPUTextureGL() override;

  static std::shared_ptr<GPUTextureGL> Create(
      const GPUTextureDescriptor& descriptor);

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override;

  uint32_t GetGLTextureID() const { return texture_id_; }

  void Initialize();
  virtual void Bind() const;
  virtual void Unbind() const;
  void Destroy();

  void CombineSampler(GPUSamplerGL* sampler);

  size_t GetBytes() const override;

  void SetFramebuffer(uint32_t fbo_id, bool need_free);

  const std::optional<GLFramebufferHolder>& GetFramebuffer() const {
    return fbo_;
  }

  SKT_BACKEND_CAST(GPUTextureGL, GPUTexture)

 private:
  uint32_t texture_target_ = 0;
  uint32_t texture_id_ = 0;
  GPUSamplerGL* combined_sampler_ = nullptr;

  std::optional<GLFramebufferHolder> fbo_ = std::nullopt;

  friend class GPUExternalTextureGL;
};

class GPUTexturePlaceholderGL : public GPUTextureGL {
 public:
  explicit GPUTexturePlaceholderGL(const GPUTextureDescriptor& descriptor)
      : GPUTextureGL(descriptor) {}

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override {}

  void Bind() const override {}

  void Unbind() const override {}

  size_t GetBytes() const override { return 0; }
};

class GPUExternalTextureGL : public GPUTextureGL {
 public:
  explicit GPUExternalTextureGL(const GPUTextureDescriptor& descriptor,
                                bool owned_by_engine, ReleaseCallback callback,
                                ReleaseUserData user_data)
      : GPUTextureGL(descriptor), owned_by_engine_(owned_by_engine) {
    SetRelease(callback, user_data);
  }

  ~GPUExternalTextureGL() override;

  static std::shared_ptr<GPUTexture> Make(
      const GPUTextureDescriptor& descriptor, uint32_t id, bool owned_by_engine,
      ReleaseCallback callback = nullptr, ReleaseUserData user_data = nullptr);

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override {}

 private:
  bool owned_by_engine_;
};

class GPUTextureRenderBufferGL : public GPUTexture {
 public:
  explicit GPUTextureRenderBufferGL(const GPUTextureDescriptor& desc,
                                    uint32_t buffer_id)
      : GPUTexture(desc), buffer_id_(buffer_id) {}

  ~GPUTextureRenderBufferGL() override;

  void UploadData(uint32_t offset_x, uint32_t offset_y, uint32_t width,
                  uint32_t height, void* data) override {}

  size_t GetBytes() const override;

  uint32_t GetBufferId() const { return buffer_id_; }

  void SetFramebuffer(uint32_t fbo_id, bool need_free);

  const std::optional<GLFramebufferHolder>& GetFramebuffer() const {
    return fbo_;
  }

  static std::shared_ptr<GPUTextureRenderBufferGL> Create(
      const GPUTextureDescriptor& desc);

 private:
  uint32_t buffer_id_;

  std::optional<GLFramebufferHolder> fbo_ = std::nullopt;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_TEXTURE_GL_HPP
