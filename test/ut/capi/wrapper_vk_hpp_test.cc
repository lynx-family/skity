// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// Compile + argument-validation smoke gate for the Vulkan-only header-only
// wrappers (skity_vk.hpp aggregate). The cases below need no Vulkan device:
// they assert the failure paths that are deterministic without one. The
// full create / present flow is exercised by the golden Vulkan tests; this
// TU belongs to the unified-only skity_wrapper_unit_test binary.

#include <skity/c/skity_surface_vk.h>
#include <skity/c/skity_texture_vk.h>
#include <vulkan/vulkan.h>

#include <skity/skity_vk.hpp>

#include "gtest/gtest.h"

namespace {

using skity::Context;
using skity::CreateVkContext;
using skity::GPUPresenter;
using skity::GPUPresenterStatus;

}  // namespace

TEST(WrapperVkHpp, ContextFactoriesValidateArguments) {
  // A null proc-address loader is rejected by the C layer (null result).
  EXPECT_EQ(CreateVkContext(nullptr), nullptr);

  // A supplied logical device without its proc-address loader is rejected.
  skity_context_create_info_vk info = {};
  info.logical_device = reinterpret_cast<VkDevice>(0x1);
  EXPECT_EQ(CreateVkContext(info), nullptr);
}

TEST(WrapperVkHpp, ChainedCreateInfoRejectsNullContext) {
  // The VK CreateInfo structs are plain p_next chains (there are no VK
  // chaining helpers): chain the extension and go through the neutral
  // members — both fail cleanly on the empty context.
  Context context;

  skity_surface_create_info_vk ext = {};
  ext.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_VK;
  skity_surface_create_info base = {};
  base.s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO;
  base.p_next = &ext;
  EXPECT_EQ(context.CreateSurface(base), nullptr);

  skity_backend_texture_info_vk tex_ext = {};
  tex_ext.s_type = SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO_VK;
  skity_backend_texture_info tex_base = {};
  tex_base.s_type = SKITY_STRUCTURE_TYPE_BACKEND_TEXTURE_INFO;
  tex_base.p_next = &tex_ext;
  tex_base.backend = SKITY_GPU_BACKEND_TYPE_VULKAN;
  EXPECT_EQ(context.WrapTexture(tex_base), nullptr);
}

TEST(WrapperVkHpp, PresenterLifecycleValidation) {
  GPUPresenter presenter;  // empty (never created)
  EXPECT_EQ(presenter.GetWidth(), 0u);
  EXPECT_EQ(presenter.GetHeight(), 0u);
  EXPECT_EQ(presenter.Resize(100, 100), SKITY_ERROR_INVALID_HANDLE);
  EXPECT_FALSE(presenter);

  // Acquire on an empty presenter fails with an error status and no
  // surface (the legacy GPUSurfaceAcquireResult shape).
  auto acquired = presenter.AcquireNextSurface({1, 1.f});
  EXPECT_EQ(acquired.status, GPUPresenterStatus::kError);
  EXPECT_EQ(acquired.surface, nullptr);

  // Present requires a non-empty surface; the empty wrapper is rejected
  // without touching the (null) presenter handle (legacy status shape).
  EXPECT_EQ(presenter.Present(nullptr), GPUPresenterStatus::kError);

  // Creation with a null context fails cleanly.
  Context null_context;
  skity_native_window_create_info_vk info = {};
  EXPECT_EQ(GPUPresenter::Create(null_context, info), nullptr);

  // Adopting a raw NULL handle is equally safe.
  EXPECT_FALSE(GPUPresenter::Adopt(nullptr));
}
