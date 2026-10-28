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
option(WGX_SPIRV_BACKEND "option for WGX SPIR-V generation"
       ${SKITY_VK_BACKEND})

if(SKITY_VK_BACKEND AND NOT WGX_SPIRV_BACKEND)
  message(FATAL_ERROR "SKITY_VK_BACKEND requires WGX_SPIRV_BACKEND")
endif()

option(SKITY_CAPI_MODULE "option for build C API wrapper module" ON)

# codec (and through it io) reach the core only via the C ABI, whose entry
# points live in the capi module
cmake_dependent_option(
  SKITY_CODEC_MODULE "option for build codec module"
  ON
  [[SKITY_CAPI_MODULE]]
  OFF
)
cmake_dependent_option(
  SKITY_IO_MODULE "option for build io module"
  ON
  [[SKITY_CODEC_MODULE]]
  OFF
)

# Single-library shape (see module/capi/README.md): the C API sources compile
# into libskity itself, the legacy C++ exports are hidden so libskity exports
# only the skity_* C ABI, and the header-only C++ wrapper under
# module/capi/wrapper/include/skity/ becomes the C++ surface — its headers
# spell the include paths EXACTLY like the legacy ones (<skity/skity.hpp>,
# <skity/graphic/paint.hpp>, ...), the build shape deciding which tree
# resolves them. OFF keeps
# today's layout — C++ API exported, skity-capi as a separate library, the
# wrapper headers neither built nor installed. The two shapes are mutually
# exclusive.
cmake_dependent_option(
  SKITY_UNIFIED_LIB "single libskity exporting only the C ABI; C++ through the header-only wrapper"
  OFF
  [[SKITY_CAPI_MODULE]]
  OFF
)

# option for building example
# User can pass -DSKITY_EXAMPLE=ON to build example if needed
option(SKITY_EXAMPLE "option for building example" OFF)

# option for building test
# User can pass -DSKITY_TEST=ON to build test if needed
option(SKITY_TEST "option for building test" OFF)

cmake_dependent_option(
  SKITY_TEST_UT "option for building unit test"
  ON
  [[SKITY_TEST]]
  OFF
)

cmake_dependent_option(
  SKITY_VK_TEST_USE_SYSTEM_LOADER "option for linking vulkan unit tests with system loader"
  OFF
  [[SKITY_VK_BACKEND AND SKITY_TEST_UT]]
  OFF
)

cmake_dependent_option(
  SKITY_TEST_BENCH "option for building benchmark test"
  ON
  [[SKITY_TEST]]
  OFF
)

cmake_dependent_option(
  SKITY_TEST_GOLDEN "option for building golden test"
  ON
  [[SKITY_TEST]]
  OFF
)

# option for generate code coverage report
# Note:
#  We only support generate code coverage report by GNU or Clang compiler
cmake_dependent_option(
  SKITY_TEST_COVERAGE "option for generate test code coverage report"
  OFF
  [[SKITY_TEST]]
  OFF
)

cmake_dependent_option(
  SKITY_TEST_TSAN "option for enabling ThreadSanitizer in unit tests"
  OFF
  [[SKITY_TEST AND SKITY_TEST_UT AND NOT SKITY_TEST_BENCH AND NOT SKITY_TEST_GOLDEN AND NOT SKITY_TEST_COVERAGE]]
  OFF
)

option(SKITY_LOG "option for logging" OFF)
option(SKITY_CT_FONT "option for open CoreText font backend on Darwin" OFF)
set(SKITY_FONTCONFIG_DEFAULT OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT ANDROID AND NOT SKITY_CT_FONT)
  set(SKITY_FONTCONFIG_DEFAULT ON)
endif()
option(SKITY_FONTCONFIG "Use the native Linux Fontconfig font manager"
       ${SKITY_FONTCONFIG_DEFAULT})
unset(SKITY_FONTCONFIG_DEFAULT)
if(SKITY_FONTCONFIG AND (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR ANDROID OR SKITY_CT_FONT))
  message(FATAL_ERROR "SKITY_FONTCONFIG requires Linux and the FreeType backend")
endif()

option(SKITY_ENABLE_FONT_HARNESS "option for building font harness CLI and tests" OFF)

option(SKITY_USE_SELF_LIBCXX "option to force skity use self libcxx" OFF)
option(SKITY_TRACE "option for enable skity tracing" OFF)
option(SKITY_BENCH_ENABLE_PERFETTO "option for enable perfetto tracing for benchmark" OFF)

if (SKITY_BENCH_ENABLE_PERFETTO)
  set(SKITY_TRACE ON CACHE BOOL "Force enable skity trace for perfetto" FORCE)
endif()

option(SKITY_USE_ASAN "option for enable skity use asan" OFF)
option(SKITY_CMAKE_TO_GN "option for generate skity.gni" OFF)

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

# option for generate install configs
cmake_dependent_option(
  SKITY_BUILD_INSTALL "option for generate install configs"
  ON
  [[NOT EMSCRIPTEN]]
  OFF
)
