# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include(CMakeDependentOption)

option(SKITY_HW_RENDERER "option for gpu raster" ON)
option(SKITY_MTL_BACKEND "option for metal raster" OFF)
option(SKITY_SW_RENDERER "option for cpu raster" ON)
option(SKITY_GL_BACKEND "option for opengl backend" ON)

option(SKITY_VK_BACKEND "option for vulkan backend" OFF)
option(SKITY_CODEC_MODULE "option for build codec module" ON)

option(SKITY_EXAMPLE "option for building example" OFF)
option(SKITY_TEST "option for building test" OFF)

option(SKITY_LOG "option for logging" OFF)
option(SKITY_CT_FONT "option for open CoreText font backend on Darwin" OFF)

option(SKITY_USE_MESA_GLES "option to force skity use GLES as backend" OFF)
option(SKITY_USE_SELF_LIBCXX "option to force skity use self libcxx" OFF)
option(SKITY_TRACE "option for enable skity tracing" OFF)
option(SKITY_USE_ASAN "option for enable skity use asan" OFF)

# Since Android needs FunctorView feature, and this feature require vulkan backend
if (ANDROID AND NOT SKITY_VK_BACKEND)
  # Force enable vulkan backend
  set(SKITY_VK_BACKEND ON)
endif()
