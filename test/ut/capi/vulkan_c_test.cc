// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_context_vk.h>
#include <skity_c/skity_native_window_vk.h>
#include <skity_c/skity_semaphore_vk.h>
#include <skity_c/skity_surface_vk.h>
#include <skity_c/skity_texture_vk.h>
#include <vulkan/vulkan.h>

#include "gtest/gtest.h"

TEST(VulkanCAPITest, ContextCreationValidatesArguments) {
  skity_context context = nullptr;
  EXPECT_EQ(skity_context_create_vk(nullptr, &context),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(skity_context_create_vk_ex(nullptr, &context),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(context, nullptr);

  skity_context_create_info_vk info = {};
  info.logical_device = reinterpret_cast<VkDevice>(0x1);
  EXPECT_EQ(skity_context_create_vk_ex(&info, &context),
            SKITY_ERROR_INVALID_ARGUMENT);
  EXPECT_EQ(context, nullptr);
}

TEST(VulkanCAPITest, SemaphoreAndSurfaceValidateHandles) {
  EXPECT_EQ(skity_semaphore_create(nullptr), nullptr);
  EXPECT_EQ(skity_semaphore_import(nullptr, nullptr, nullptr),
            SKITY_ERROR_INVALID_HANDLE);
  // A base info without a backend extension is rejected as unsupported.
  skity_semaphore_import_info info = {};
  info.s_type = SKITY_STRUCTURE_TYPE_SEMAPHORE_IMPORT_INFO;
  EXPECT_EQ(skity_semaphore_import(nullptr, nullptr, &info),
            SKITY_ERROR_INVALID_HANDLE);
  skity_surface surface = nullptr;
  skity_semaphore semaphore = nullptr;
  skity_surface_add_external_wait_semaphore(surface, semaphore);
  EXPECT_EQ(
      skity_native_window_acquire_next_surface_vk(nullptr, 1, 1.f, &surface),
      SKITY_ERROR_INVALID_HANDLE);
  EXPECT_EQ(skity_native_window_present_vk(nullptr, &surface),
            SKITY_ERROR_INVALID_HANDLE);
}
