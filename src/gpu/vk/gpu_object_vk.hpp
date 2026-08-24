// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_VK_GPU_OBJECT_VK_HPP
#define SRC_GPU_VK_GPU_OBJECT_VK_HPP

#include <memory>

#include "src/gpu/vk/vulkan_context_state.hpp"

namespace skity {
/**
 * Base class for Vulkan GPU objects that need access to the
 * VulkanContextState to destroy their Vulkan handles.
 *
 * The state is held weakly on purpose: pending submissions retain GPU
 * objects (textures, samplers, buffers, ...) until GPU completion via
 * cleanup actions. A strong reference would create a reference cycle:
 * state -> pending submission -> cleanup action -> GPU object -> state,
 * which prevents the state from being destroyed and leads to re-entrant
 * destruction of the VulkanContextState when the cycle is torn down.
 */
class GPUObjectVK {
 public:
  explicit GPUObjectVK(std::shared_ptr<const VulkanContextState> state)
      : state_(std::move(state)) {}

  virtual ~GPUObjectVK() = default;

 protected:
  /**
   * Returns the shared state if it is still alive, nullptr otherwise.
   * When nullptr is returned (e.g. during context teardown), Vulkan handle
   * destruction must be skipped since the device/allocator are gone.
   */
  std::shared_ptr<const VulkanContextState> LockState() const {
    const auto state = state_.lock();
    if (state == nullptr || state->IsDeviceLost()) {
      // A lost device invalidates every Vulkan object created on it.
      // Destroying handles against it is UB and crashes some drivers, so
      // treat the state as gone and skip destruction.
      return {};
    }
    return state;
  }

 private:
  std::weak_ptr<const VulkanContextState> state_;
};

}  // namespace skity

#endif  // SRC_GPU_VK_GPU_OBJECT_VK_HPP
