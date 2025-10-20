# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include(CMakeDependentOption)

option(SKITY_HW_RENDERER "option for gpu raster" ON)
option(SKITY_SW_RENDERER "option for cpu raster" ON)
option(SKITY_GL_BACKEND "option for opengl backend" ON)

# option for metal raster open on APPLE platform by default
# User can pass -DSKITY_MTL_BACKEND=OFF to disable metal backend if needed
cmake_dependent_option(
  SKITY_MTL_BACKEND "option for metal raster open on APPLE platform by default"
  ON
  [[APPLE OR CMAKE_SYSTEM_NAME STREQUAL "iOS" OR CMAKE_SYSTEM_NAME STREQUAL "Darwin"]]
  OFF
)

option(SKITY_VK_BACKEND "option for vulkan backend" OFF)
option(SKITY_CODEC_MODULE "option for build codec module" ON)
cmake_dependent_option(
  SKITY_IO_MODULE "option for build io module"
  ON
  [[SKITY_CODEC_MODULE]]
  OFF
)

# option for building example
# User can pass -DSKITY_EXAMPLE=ON to build example if needed
option(SKITY_EXAMPLE "option for building example" OFF)

# option for building test, it controls both unit test , benchmark and golden test
# User can pass -DSKITY_TEST=ON to build test if needed
# Note:
#  The golden test code can only run on APPLE platform
option(SKITY_TEST "option for building test" OFF)

# option for generate code coverage report
# Note:
#  We only support generate code coverage report by GNU or Clang compiler
cmake_dependent_option(
  SKITY_TEST_COVERAGE "option for generate test code coverage report"
  OFF
  [[SKITY_TEST]]
  OFF
)

option(SKITY_LOG "option for logging" OFF)
option(SKITY_CT_FONT "option for open CoreText font backend on Darwin" OFF)

option(SKITY_USE_SELF_LIBCXX "option to force skity use self libcxx" OFF)
option(SKITY_TRACE "option for enable skity tracing" OFF)
option(SKITY_USE_ASAN "option for enable skity use asan" OFF)

cmake_dependent_option(
  SKITY_OPTIMIZE_O3 "Use '-O3' optimization flag in Release mode"
  OFF
  [[CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"]]
  OFF
)


# option for generate skity framework
# This is only valid on APPLE platform
cmake_dependent_option(
  SKITY_GENERATE_FRAMEWORK "option for generate skity framework"
  OFF
  [[APPLE]]
  OFF
)
