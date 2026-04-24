// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_VULKAN_RENDER_PASS_CACHE_HPP
#define SRC_GPU_VK_VULKAN_RENDER_PASS_CACHE_HPP

#include <vulkan/vulkan.h>

#include <vector>

namespace skity {

class VulkanContextState;

class VulkanRenderPassCache {
 public:
  struct Key {
    VkFormat color_format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits color_samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp color_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp color_store_op = VK_ATTACHMENT_STORE_OP_STORE;
    bool has_resolve = false;
    VkFormat resolve_format = VK_FORMAT_UNDEFINED;
    VkAttachmentStoreOp resolve_store_op = VK_ATTACHMENT_STORE_OP_STORE;
    bool has_depth = false;
    bool has_stencil = false;
    VkFormat depth_stencil_format = VK_FORMAT_UNDEFINED;
    VkSampleCountFlagBits depth_stencil_samples = VK_SAMPLE_COUNT_1_BIT;
    VkAttachmentLoadOp depth_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp depth_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    VkAttachmentLoadOp stencil_load_op = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    VkAttachmentStoreOp stencil_store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    bool operator==(const Key& other) const;
  };

  VulkanRenderPassCache();
  ~VulkanRenderPassCache() = default;

  VkRenderPass GetOrCreate(const VulkanContextState& state, const Key& key);

  void Reset(const VulkanContextState& state);

 private:
  struct Entry {
    Key key = {};
    VkRenderPass render_pass = VK_NULL_HANDLE;
  };

  std::vector<Entry> render_passes_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_VK_VULKAN_RENDER_PASS_CACHE_HPP
