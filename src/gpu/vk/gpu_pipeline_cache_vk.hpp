// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_PIPELINE_CACHE_VK_HPP
#define SRC_GPU_VK_GPU_PIPELINE_CACHE_VK_HPP

#include <volk.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace skity {

class GPUDeviceVk;
class GPURenderPipelineVk;
struct GPURenderPipelineDescriptor;

// Hash key for pipeline caching
struct PipelineKey {
  std::string vertex_shader_hash;
  std::string fragment_shader_hash;
  std::vector<uint8_t> vertex_layout_hash;
  uint32_t render_state_hash;

  bool operator==(const PipelineKey& other) const {
    return vertex_shader_hash == other.vertex_shader_hash &&
           fragment_shader_hash == other.fragment_shader_hash &&
           vertex_layout_hash == other.vertex_layout_hash &&
           render_state_hash == other.render_state_hash;
  }
};

struct PipelineKeyHash {
  size_t operator()(const PipelineKey& key) const {
    size_t h1 = std::hash<std::string>{}(key.vertex_shader_hash);
    size_t h2 = std::hash<std::string>{}(key.fragment_shader_hash);
    size_t h3 = std::hash<uint32_t>{}(key.render_state_hash);

    // Simple hash combination
    return h1 ^ (h2 << 1) ^ (h3 << 2);
  }
};

class GPUPipelineCacheVk {
 public:
  explicit GPUPipelineCacheVk(GPUDeviceVk* device);
  ~GPUPipelineCacheVk();

  bool Initialize();
  void Destroy();

  // Get or create a cached pipeline
  std::unique_ptr<GPURenderPipelineVk> GetOrCreatePipeline(
      const GPURenderPipelineDescriptor& desc);

  // Save cache to disk
  bool SaveCache(const std::string& file_path);

  // Load cache from disk
  bool LoadCache(const std::string& file_path);

  // Clear all cached pipelines
  void ClearCache();

  // Get cache statistics
  size_t GetCacheSize() const { return pipeline_cache_.size(); }
  size_t GetHitCount() const { return cache_hits_; }
  size_t GetMissCount() const { return cache_misses_; }

  VkPipelineCache GetVkPipelineCache() const { return vk_pipeline_cache_; }

 private:
  PipelineKey CreatePipelineKey(const GPURenderPipelineDescriptor& desc);
  uint32_t HashRenderState(const GPURenderPipelineDescriptor& desc);
  std::vector<uint8_t> HashVertexLayout(
      const GPURenderPipelineDescriptor& desc);

  GPUDeviceVk* device_ = nullptr;
  VkPipelineCache vk_pipeline_cache_ = VK_NULL_HANDLE;

  std::unordered_map<PipelineKey, std::shared_ptr<GPURenderPipelineVk>,
                     PipelineKeyHash>
      pipeline_cache_;

  // Statistics
  mutable size_t cache_hits_ = 0;
  mutable size_t cache_misses_ = 0;

  bool initialized_ = false;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_PIPELINE_CACHE_VK_HPP