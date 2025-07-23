// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GL_GPU_SAMPLER_GL_HPP
#define SRC_GPU_GL_GPU_SAMPLER_GL_HPP

#include <memory>

#include "src/gpu/backend_cast.hpp"
#include "src/gpu/gpu_sampler.hpp"

namespace skity {

class GPUTextureGL;

class GPUSamplerGL : public GPUSampler {
 public:
  explicit GPUSamplerGL(const GPUSamplerDescriptor& descriptor);

  ~GPUSamplerGL() override;

  static std::shared_ptr<GPUSamplerGL> Create(
      const GPUSamplerDescriptor& descriptor);

  void ConfigureTexture(GPUTextureGL* texture) const;

  uint32_t GetSamplerID() const { return sampler_id_; }

  SKT_BACKEND_CAST(GPUSamplerGL, GPUSampler)

 private:
  uint32_t sampler_id_ = 0;
};

}  // namespace skity

#endif  // SRC_GPU_GL_GPU_SAMPLER_GL_HPP
