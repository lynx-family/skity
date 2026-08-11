// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_H

/*
 * Aggregate header for the Skity C API. Include this for the full surface.
 * Vulkan context creation (skity_context_create_vk) is in skity_context_vk.h,
 * which is intentionally separate so that non-Vulkan consumers are not forced
 * to pull in <vulkan/vulkan.h>.
 */

#include <skity_c/skity_base.h>
#include <skity_c/skity_bitmap.h>
#include <skity_c/skity_camera.h>
#include <skity_c/skity_canvas.h>
#include <skity_c/skity_color_filter.h>
#include <skity_c/skity_context.h>
#include <skity_c/skity_data.h>
#include <skity_c/skity_font.h>
#include <skity_c/skity_image.h>
#include <skity_c/skity_image_filter.h>
#include <skity_c/skity_mask_filter.h>
#include <skity_c/skity_paint.h>
#include <skity_c/skity_path.h>
#include <skity_c/skity_path_effect.h>
#include <skity_c/skity_path_measure.h>
#include <skity_c/skity_path_op.h>
#include <skity_c/skity_precompile.h>
#include <skity_c/skity_quaternion.h>
#include <skity_c/skity_recorder.h>
#include <skity_c/skity_shader.h>
#include <skity_c/skity_stroke.h>
#include <skity_c/skity_surface.h>
#include <skity_c/skity_text.h>
#include <skity_c/skity_texture.h>
#include <skity_c/skity_types.h>

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_H
