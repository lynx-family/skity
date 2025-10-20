// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/vk/gpu_pipeline_cache_vk.hpp"

#include <fstream>
#include <functional>

#include "src/gpu/vk/gpu_device_vk.hpp"
#include "src/gpu/vk/gpu_render_pipeline_vk.hpp"
#include "src/gpu/vk/gpu_shader_function_vk.hpp"
#include "src/logging.hpp"

namespace skity {

GPUPipelineCacheVk::GPUPipelineCacheVk(GPUDeviceVk* device) : device_(device) {}

GPUPipelineCacheVk::~GPUPipelineCacheVk() { Destroy(); }

bool GPUPipelineCacheVk::Initialize() {
  if (initialized_) {
    return true;
  }

  VkDevice vk_device = device_->GetDevice();

  // Create Vulkan pipeline cache
  VkPipelineCacheCreateInfo cache_info = {};
  cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  cache_info.initialDataSize = 0;
  cache_info.pInitialData = nullptr;

  VkResult result = vkCreatePipelineCache(vk_device, &cache_info, nullptr,
                                          &vk_pipeline_cache_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create Vulkan pipeline cache: %d", result);
    return false;
  }

  initialized_ = true;
  LOGI("Pipeline cache initialized successfully");
  return true;
}

void GPUPipelineCacheVk::Destroy() {
  if (!initialized_) {
    return;
  }

  VkDevice vk_device = device_->GetDevice();

  if (vk_pipeline_cache_ != VK_NULL_HANDLE) {
    vkDestroyPipelineCache(vk_device, vk_pipeline_cache_, nullptr);
    vk_pipeline_cache_ = VK_NULL_HANDLE;
  }

  pipeline_cache_.clear();
  initialized_ = false;

  LOGI(
      "Pipeline cache destroyed - Final stats: %zu hits, %zu misses, %zu total "
      "pipelines",
      cache_hits_, cache_misses_, pipeline_cache_.size());
}

std::unique_ptr<GPURenderPipelineVk> GPUPipelineCacheVk::GetOrCreatePipeline(
    const GPURenderPipelineDescriptor& desc) {
  // Create key for this pipeline configuration
  PipelineKey key = CreatePipelineKey(desc);

  // Check if pipeline already exists in cache
  auto it = pipeline_cache_.find(key);
  if (it != pipeline_cache_.end()) {
    cache_hits_++;
    LOGI("Pipeline cache hit! (Total hits: %zu)", cache_hits_);
    // For caching, we need to clone the pipeline since we can't return the same
    // unique_ptr In practice, this would involve proper pipeline sharing or
    // using a different caching strategy
    return GPURenderPipelineVk::Create(device_, desc);
  }

  // Cache miss - create new pipeline
  cache_misses_++;
  LOGI("Pipeline cache miss - creating new pipeline (Total misses: %zu)",
       cache_misses_);

  auto pipeline = GPURenderPipelineVk::Create(device_, desc);
  if (!pipeline) {
    LOGE("Failed to create pipeline for caching");
    return nullptr;
  }

  // Store a reference in cache for future hit detection
  // Note: This is a simplified caching approach
  pipeline_cache_[key] = std::shared_ptr<GPURenderPipelineVk>(
      pipeline.get(), [](GPURenderPipelineVk*) {});

  LOGI("Pipeline cached successfully. Cache size: %zu", pipeline_cache_.size());
  return pipeline;
}

bool GPUPipelineCacheVk::SaveCache(const std::string& file_path) {
  if (!initialized_ || vk_pipeline_cache_ == VK_NULL_HANDLE) {
    LOGE("Pipeline cache not initialized for saving");
    return false;
  }

  VkDevice vk_device = device_->GetDevice();

  // Get cache data size
  size_t cache_size = 0;
  VkResult result = vkGetPipelineCacheData(vk_device, vk_pipeline_cache_,
                                           &cache_size, nullptr);
  if (result != VK_SUCCESS || cache_size == 0) {
    LOGW("No pipeline cache data to save");
    return false;
  }

  // Get cache data
  std::vector<uint8_t> cache_data(cache_size);
  result = vkGetPipelineCacheData(vk_device, vk_pipeline_cache_, &cache_size,
                                  cache_data.data());
  if (result != VK_SUCCESS) {
    LOGE("Failed to get pipeline cache data: %d", result);
    return false;
  }

  // Write to file
  std::ofstream file(file_path, std::ios::binary);
  if (!file.is_open()) {
    LOGE("Failed to open cache file for writing: %s", file_path.c_str());
    return false;
  }

  file.write(reinterpret_cast<const char*>(cache_data.data()), cache_size);
  file.close();

  LOGI("Pipeline cache saved to %s (%zu bytes)", file_path.c_str(), cache_size);
  return true;
}

bool GPUPipelineCacheVk::LoadCache(const std::string& file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    LOGW("Pipeline cache file not found: %s", file_path.c_str());
    return false;
  }

  size_t file_size = file.tellg();
  if (file_size == 0) {
    LOGW("Empty pipeline cache file: %s", file_path.c_str());
    return false;
  }

  file.seekg(0);
  std::vector<uint8_t> cache_data(file_size);
  file.read(reinterpret_cast<char*>(cache_data.data()), file_size);
  file.close();

  // Destroy existing cache and create new one with loaded data
  if (vk_pipeline_cache_ != VK_NULL_HANDLE) {
    vkDestroyPipelineCache(device_->GetDevice(), vk_pipeline_cache_, nullptr);
  }

  VkPipelineCacheCreateInfo cache_info = {};
  cache_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  cache_info.initialDataSize = file_size;
  cache_info.pInitialData = cache_data.data();

  VkResult result = vkCreatePipelineCache(device_->GetDevice(), &cache_info,
                                          nullptr, &vk_pipeline_cache_);
  if (result != VK_SUCCESS) {
    LOGE("Failed to create pipeline cache with loaded data: %d", result);
    return false;
  }

  LOGI("Pipeline cache loaded from %s (%zu bytes)", file_path.c_str(),
       file_size);
  return true;
}

void GPUPipelineCacheVk::ClearCache() {
  pipeline_cache_.clear();
  cache_hits_ = 0;
  cache_misses_ = 0;
  LOGI("Pipeline cache cleared");
}

PipelineKey GPUPipelineCacheVk::CreatePipelineKey(
    const GPURenderPipelineDescriptor& desc) {
  PipelineKey key;

  // Hash shader functions
  if (desc.vertex_function) {
    key.vertex_shader_hash = desc.vertex_function->GetLabel();
  }
  if (desc.fragment_function) {
    key.fragment_shader_hash = desc.fragment_function->GetLabel();
  }

  // Hash vertex layout
  key.vertex_layout_hash = HashVertexLayout(desc);

  // Hash render state
  key.render_state_hash = HashRenderState(desc);

  return key;
}

uint32_t GPUPipelineCacheVk::HashRenderState(
    const GPURenderPipelineDescriptor& desc) {
  // Simple hash of render state properties
  uint32_t hash = 0;
  hash ^= std::hash<int>{}(static_cast<int>(desc.target.format));
  hash ^= std::hash<int>{}(static_cast<int>(desc.target.src_blend_factor)) << 1;
  hash ^= std::hash<int>{}(static_cast<int>(desc.target.dst_blend_factor)) << 2;
  hash ^= std::hash<int>{}(desc.target.write_mask) << 3;
  hash ^= std::hash<int>{}(desc.sample_count) << 4;
  hash ^= std::hash<bool>{}(desc.depth_stencil.enable_depth) << 5;
  hash ^= std::hash<bool>{}(desc.depth_stencil.enable_stencil) << 6;
  return hash;
}

std::vector<uint8_t> GPUPipelineCacheVk::HashVertexLayout(
    const GPURenderPipelineDescriptor& desc) {
  std::vector<uint8_t> hash_data;

  for (const auto& buffer : desc.buffers) {
    // Hash stride
    auto stride_bytes = reinterpret_cast<const uint8_t*>(&buffer.array_stride);
    hash_data.insert(hash_data.end(), stride_bytes,
                     stride_bytes + sizeof(buffer.array_stride));

    // Hash step mode
    uint8_t step_mode = static_cast<uint8_t>(buffer.step_mode);
    hash_data.push_back(step_mode);

    // Hash attributes
    for (const auto& attr : buffer.attributes) {
      auto format_bytes = reinterpret_cast<const uint8_t*>(&attr.format);
      hash_data.insert(hash_data.end(), format_bytes,
                       format_bytes + sizeof(attr.format));

      auto offset_bytes = reinterpret_cast<const uint8_t*>(&attr.offset);
      hash_data.insert(hash_data.end(), offset_bytes,
                       offset_bytes + sizeof(attr.offset));

      auto location_bytes =
          reinterpret_cast<const uint8_t*>(&attr.shader_location);
      hash_data.insert(hash_data.end(), location_bytes,
                       location_bytes + sizeof(attr.shader_location));
    }
  }

  return hash_data;
}

}  // namespace skity