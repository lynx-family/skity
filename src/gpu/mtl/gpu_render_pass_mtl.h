// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_MTL_GPU_RENDER_PASS_MTL_H
#define SRC_GPU_MTL_GPU_RENDER_PASS_MTL_H

#import <Metal/Metal.h>

#include <map>
#include <vector>

#include "src/gpu/gpu_render_pass.hpp"

namespace skity {

class GPURenderPassMTL : public GPURenderPass {
 private:
  struct BindingsCache {
    explicit BindingsCache(id<MTLRenderCommandEncoder> encoder)
        : encoder_(encoder) {}

    BindingsCache(const BindingsCache&) = delete;

    BindingsCache(BindingsCache&&) = delete;

    void SetRenderPipelineState(id<MTLRenderPipelineState> pipeline);

    void SetDepthStencilState(id<MTLDepthStencilState> depth_stencil);

    void SetBuffer(GPUShaderStage stage, uint64_t index, uint64_t offset,
                   id<MTLBuffer> buffer);

    void SetTexture(GPUShaderStage stage, uint64_t index,
                    id<MTLTexture> texture);

    void SetSampler(GPUShaderStage stage, uint64_t index,
                    id<MTLSamplerState> sampler);

   private:
    struct BufferOffsetPair {
      id<MTLBuffer> buffer = nullptr;
      size_t offset = 0u;
    };
    using BufferMap = std::map<uint64_t, BufferOffsetPair>;
    using TextureMap = std::map<uint64_t, id<MTLTexture>>;
    using SamplerMap = std::map<uint64_t, id<MTLSamplerState>>;

    const id<MTLRenderCommandEncoder> encoder_;
    id<MTLRenderPipelineState> pipeline_ = nullptr;
    id<MTLDepthStencilState> depth_stencil_ = nullptr;
    std::map<GPUShaderStage, BufferMap> buffers_;
    std::map<GPUShaderStage, TextureMap> textures_;
    std::map<GPUShaderStage, SamplerMap> samplers_;
  };

 public:
  GPURenderPassMTL(id<MTLRenderCommandEncoder> encoder,
                   const GPURenderPassDescriptor& desc,
                   bool auto_end_encoding = false)
      : GPURenderPass(desc),
        encoder_(encoder),
        auto_end_encoding_(auto_end_encoding) {}

  void EncodeCommands(std::optional<GPUViewport> viewport,
                      std::optional<GPUScissorRect> scissor) override;

 private:
  void SetStencilReference(uint32_t reference);

  void SetPipeline(BindingsCache& cache, GPURenderPipeline* pipeline);

  void SetUniformBindings(BindingsCache& cache,
                          const ArrayList<UniformBinding, 4>& uniform_bindings);

  void SetTextureBindings(
      BindingsCache& cache,
      const ArrayList<TextureSamplerBinding, 4>& texture_bindings);

  void SetTextureBindings(BindingsCache& cache,
                          const ArrayList<TextureBinding, 4>& bindings);

  void SetSamplerBindings(BindingsCache& cache,
                          const ArrayList<SamplerBinding, 4>& bindings);

  id<MTLRenderCommandEncoder> encoder_;
  bool auto_end_encoding_;
};

}  // namespace skity

#endif  // SRC_GPU_MTL_GPU_RENDER_PASS_MTL_H
