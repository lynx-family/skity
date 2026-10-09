// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_VK_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_VK_HPP

// VK aggregator of the header-only RAII layer. Vulkan consumers include
// <skity_hpp/skity_vk.hpp> INSTEAD of (or in addition to)
// <skity_hpp/skity.hpp>: it pulls in the whole neutral layer plus the
// Vulkan-only wrapper headers below. The VK headers are deliberately kept
// out of skity.hpp so GL / CPU consumers never see <vulkan/vulkan.h>.
//
// Same usage rules as skity.hpp: do not include this header in a TU that
// also includes the legacy headers under include/skity/.

#include <skity_hpp/skity.hpp>
#include <skity_hpp/skity_context_vk.hpp>
#include <skity_hpp/skity_native_window_vk.hpp>
#include <skity_hpp/skity_semaphore_vk.hpp>
#include <skity_hpp/skity_surface_vk.hpp>
#include <skity_hpp/skity_texture_vk.hpp>

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_VK_HPP
