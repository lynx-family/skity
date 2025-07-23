// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/mtl/gpu_render_pass_mtl.h"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_shader_function.hpp"
#include "src/gpu/mtl/gpu_buffer_mtl.h"
#include "src/gpu/mtl/gpu_render_pipeline_mtl.h"
#include "src/gpu/mtl/gpu_sampler_mtl.h"
#include "src/gpu/mtl/gpu_texture_mtl.h"

namespace skity {

void GPURenderPassMTL::BindingsCache::SetRenderPipelineState(id<MTLRenderPipelineState> pipeline) {
  if (pipeline == pipeline_) {
    return;
  }
  pipeline_ = pipeline;
  [encoder_ setRenderPipelineState:pipeline_];
}

void GPURenderPassMTL::BindingsCache::SetDepthStencilState(id<MTLDepthStencilState> depth_stencil) {
  if (depth_stencil_ == depth_stencil) {
    return;
  }
  depth_stencil_ = depth_stencil;
  [encoder_ setDepthStencilState:depth_stencil_];
}

void GPURenderPassMTL::BindingsCache::SetBuffer(GPUShaderStage stage, uint64_t index,
                                                uint64_t offset, id<MTLBuffer> buffer) {
  auto& buffers_map = buffers_[stage];
  auto found = buffers_map.find(index);
  if (found != buffers_map.end() && found->second.buffer == buffer) {
    // The right buffer is bound. Check if its offset needs to be updated.
    if (found->second.offset == offset) {
      // Buffer and its offset is identical. Nothing to do.
      return;
    }

    // Only the offset needs to be updated.
    found->second.offset = offset;

    switch (stage) {
      case GPUShaderStage::kVertex:
        [encoder_ setVertexBufferOffset:offset atIndex:index];
        break;
      case GPUShaderStage::kFragment:
        [encoder_ setFragmentBufferOffset:offset atIndex:index];
        break;
    }
    return;
  }
  buffers_map[index] = {buffer, static_cast<size_t>(offset)};
  switch (stage) {
    case GPUShaderStage::kVertex:
      [encoder_ setVertexBuffer:buffer offset:offset atIndex:index];
      break;
    case GPUShaderStage::kFragment:
      [encoder_ setFragmentBuffer:buffer offset:offset atIndex:index];
      break;
  }
}

void GPURenderPassMTL::BindingsCache::SetTexture(GPUShaderStage stage, uint64_t index,
                                                 id<MTLTexture> texture) {
  auto& texture_map = textures_[stage];
  auto found = texture_map.find(index);
  if (found != texture_map.end() && found->second == texture) {
    // Already bound.
    return;
  }
  texture_map[index] = texture;
  switch (stage) {
    case GPUShaderStage::kVertex:
      [encoder_ setVertexTexture:texture atIndex:index];
      break;
    case GPUShaderStage::kFragment:
      [encoder_ setFragmentTexture:texture atIndex:index];
      break;
  }
}

void GPURenderPassMTL::BindingsCache::SetSampler(GPUShaderStage stage, uint64_t index,
                                                 id<MTLSamplerState> sampler) {
  auto& sampler_map = samplers_[stage];
  auto found = sampler_map.find(index);
  if (found != sampler_map.end() && found->second == sampler) {
    // Already bound.
    return;
  }
  sampler_map[index] = sampler;
  switch (stage) {
    case GPUShaderStage::kVertex:
      [encoder_ setVertexSamplerState:sampler atIndex:index];
      break;
    case GPUShaderStage::kFragment:
      [encoder_ setFragmentSamplerState:sampler atIndex:index];
      break;
  }
}

void GPURenderPassMTL::EncodeCommands(std::optional<GPUViewport> viewport,
                                      std::optional<GPUScissorRect> scissor) {
  BindingsCache cache(encoder_);
  auto target_width = GetDescriptor().GetTargetWidth();
  auto target_height = GetDescriptor().GetTargetHeight();

  GPUViewport v = viewport.value_or(GPUViewport{
      0,
      0,
      static_cast<float>(target_width),
      static_cast<float>(target_height),
      0,
      1,
  });
  GPUScissorRect s = scissor.value_or(GPUScissorRect{
      0,
      0,
      target_width,
      target_height,
  });

  // Set viewport
  [encoder_ setViewport:MTLViewport{
                            .originX = v.x,
                            .originY = v.y,
                            .width = static_cast<double>(v.width),
                            .height = static_cast<double>(v.height),
                            .znear = v.min_depth,
                            .zfar = v.max_depth,
                        }];
  // Set scissor
  [encoder_ setScissorRect:MTLScissorRect{
                               .x = s.x,
                               .y = s.y,
                               .width = s.width,
                               .height = s.height,
                           }];

  for (auto& command : GetCommands()) {
    if (!command->IsValid()) {
      continue;
    }
    [encoder_ setScissorRect:MTLScissorRect{.x = command->scissor_rect.x,
                                            .y = command->scissor_rect.y,
                                            .width = command->scissor_rect.width,
                                            .height = command->scissor_rect.height}];
    SetStencilReference(command->stencil_reference);
    SetPipeline(cache, command->pipeline);
    SetUniformBindings(cache, command->uniform_bindings);
    SetTextureBindings(cache, command->texture_sampler_bindings);

    // handle texture and sampler bindings separately
    SetTextureBindings(cache, command->texture_bindings);
    SetSamplerBindings(cache, command->sampler_bindings);

    cache.SetBuffer(GPUShaderStage::kVertex, 0, command->vertex_buffer.offset,
                    static_cast<GPUBufferMTL*>(command->vertex_buffer.buffer)->GetMTLBuffer());

    [encoder_ drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                         indexCount:command->index_count
                          indexType:MTLIndexTypeUInt32
                        indexBuffer:static_cast<GPUBufferMTL*>(command->index_buffer.buffer)
                                        ->GetMTLBuffer()
                  indexBufferOffset:command->index_buffer.offset];
  }

  if (auto_end_encoding_) {
    [encoder_ endEncoding];
  }
}

void GPURenderPassMTL::SetUniformBindings(BindingsCache& cache,
                                          const ArrayList<UniformBinding, 4>& uniform_bindings) {
  for (auto& binding : uniform_bindings) {
    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kVertex)) {
      cache.SetBuffer(GPUShaderStage::kVertex, binding.index, binding.buffer.offset,
                      static_cast<GPUBufferMTL*>(binding.buffer.buffer)->GetMTLBuffer());
    }
    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kFragment)) {
      cache.SetBuffer(GPUShaderStage::kFragment, binding.index, binding.buffer.offset,
                      static_cast<GPUBufferMTL*>(binding.buffer.buffer)->GetMTLBuffer());
    }
  }
}

void GPURenderPassMTL::SetTextureBindings(
    BindingsCache& cache, const ArrayList<TextureSamplerBinding, 4>& texture_bindings) {
  for (auto& binding : texture_bindings) {
    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kVertex)) {
      cache.SetTexture(GPUShaderStage::kVertex, binding.index,
                       static_cast<GPUTextureMTL*>(binding.texture.get())->GetMTLTexture());
      cache.SetSampler(GPUShaderStage::kVertex, binding.index,
                       static_cast<GPUSamplerMTL*>(binding.sampler.get())->GetMTLSampler());
    }
    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kFragment)) {
      cache.SetTexture(GPUShaderStage::kFragment, binding.index,
                       static_cast<GPUTextureMTL*>(binding.texture.get())->GetMTLTexture());
      cache.SetSampler(GPUShaderStage::kFragment, binding.index,
                       static_cast<GPUSamplerMTL*>(binding.sampler.get())->GetMTLSampler());
    }
  }
}

void GPURenderPassMTL::SetTextureBindings(BindingsCache& cache,
                                          const ArrayList<TextureBinding, 4>& bindings) {
  for (auto& binding : bindings) {
    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kVertex)) {
      cache.SetTexture(GPUShaderStage::kVertex, binding.index,
                       static_cast<GPUTextureMTL*>(binding.texture.get())->GetMTLTexture());
    }

    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kFragment)) {
      cache.SetTexture(GPUShaderStage::kFragment, binding.index,
                       static_cast<GPUTextureMTL*>(binding.texture.get())->GetMTLTexture());
    }
  }
}

void GPURenderPassMTL::SetSamplerBindings(BindingsCache& cache,
                                          const ArrayList<SamplerBinding, 4>& bindings) {
  for (auto& binding : bindings) {
    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kVertex)) {
      cache.SetSampler(GPUShaderStage::kVertex, binding.index,
                       static_cast<GPUSamplerMTL*>(binding.sampler.get())->GetMTLSampler());
    }

    if (binding.stages & static_cast<GPUShaderStageMask>(GPUShaderStage::kFragment)) {
      cache.SetSampler(GPUShaderStage::kFragment, binding.index,
                       static_cast<GPUSamplerMTL*>(binding.sampler.get())->GetMTLSampler());
    }
  }
}

void GPURenderPassMTL::SetStencilReference(uint32_t reference) {
  [encoder_ setStencilReferenceValue:reference];
}

void GPURenderPassMTL::SetPipeline(BindingsCache& cache, GPURenderPipeline* pipeline) {
  GPURenderPipelineMTL* mtl_render_pipeline = static_cast<GPURenderPipelineMTL*>(pipeline);
  cache.SetRenderPipelineState(mtl_render_pipeline->GetMTLRenderPipelineState());
  cache.SetDepthStencilState(mtl_render_pipeline->GetMTLDepthStencilState());
}
}  // namespace skity
