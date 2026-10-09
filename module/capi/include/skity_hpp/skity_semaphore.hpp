// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SEMAPHORE_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SEMAPHORE_HPP

// Backend-neutral semaphore wrapper of the header-only RAII layer (legacy
// skity::GPUSemaphore + GPUContext::CreateSemaphore / ImportSemaphore).
// Only the Vulkan backend implements semaphores today; the VK import-info
// extension lives in skity_c/skity_semaphore_vk.h and is passed through
// untyped so this header stays free of <vulkan/vulkan.h>.

#include <skity_c/skity_semaphore.h>

#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_context.hpp>

namespace skity {
namespace raii {

/** GPU synchronization semaphore (GPU-GPU sync between contexts / APIs). */
class Semaphore
    : public detail::OwnHandle<skity_semaphore, skity_semaphore_destroy> {
 public:
  /**
   * Create on @p context's backend; empty wrapper on backends without
   * semaphore support (currently Vulkan only).
   */
  static Semaphore Create(const Context& context) {
    return Semaphore(skity_semaphore_create(context.get()));
  }

  /** Adopt an owning C handle. */
  static Semaphore Adopt(skity_semaphore h) { return Semaphore(h); }

  /**
   * Re-import a native sync handle (backend extension chained through
   * @p info's p_next, e.g. skity_semaphore_import_info_vk). The handle's
   * ownership transfers to the driver; reusable across frames.
   */
  skity_result Import(const Context& context,
                      const skity_semaphore_import_info& info) {
    return skity_semaphore_import(context.get(), get(), &info);
  }

  /** The backend this semaphore was created on. */
  Context::BackendType GetBackendType() const {
    return static_cast<Context::BackendType>(
        skity_semaphore_get_backend_type(get()));
  }

 private:
  explicit Semaphore(skity_semaphore h) : OwnHandle(h) {}
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_SEMAPHORE_HPP
