# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# Vulkan-style C ABI wrapper around libskity. This is a thin forwarding layer
# that links the core skity library and re-exports a stable C surface. See
# docs/C_API_DESIGN.md for the full design.

add_library(skity-capi SHARED)
add_library(skity::capi ALIAS skity-capi)

target_include_directories(skity-capi
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# module root (for "src/..." includes) and skity public headers
target_include_directories(skity-capi PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(skity-capi PRIVATE ${SKITY_ROOT}/include)

target_link_libraries(skity-capi PUBLIC skity)

target_compile_features(skity-capi PRIVATE cxx_std_17)

# Match the core library's flags. -fno-strict-aliasing is required because the
# wrapper reinterprets the binary-compatible POD structs (skity_rect/matrix/...)
# as their skity:: C++ counterparts across the boundary.
if (MSVC)
  target_compile_options(skity-capi PRIVATE /EHs-c-)
else()
  target_compile_options(skity-capi PRIVATE
    -fno-exceptions
    -fno-rtti
    -fno-strict-aliasing
    -fvisibility=hidden
    -fvisibility-inlines-hidden
  )
endif()

# Make backend guards (SKITY_OPENGL / SKITY_VULKAN) visible to the wrapper
# sources regardless of how skity exposes those definitions.
if (${SKITY_GL_BACKEND})
  target_compile_definitions(skity-capi PRIVATE -DSKITY_OPENGL)
endif()

if (${SKITY_VK_BACKEND})
  target_compile_definitions(skity-capi PRIVATE -DSKITY_VULKAN)
endif()

# Android 16 KiB page-size alignment (arm64 / x86_64), required for the .so to
# load on Android 15+ devices with 16 KiB pages. Mirrors the option applied to
# the core skity target in cmake/Platform.cmake.
if (ANDROID)
  if (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" OR CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
    target_link_options(skity-capi PRIVATE
      "-Wl,-z,max-page-size=16384"
      "-Wl,-z,common-page-size=16384"
    )
  endif()
endif()

target_sources(
  skity-capi
  PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src/handle.hpp
    ${CMAKE_CURRENT_LIST_DIR}/src/context_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/surface_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/canvas_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/paint_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/path_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/path_measure_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/path_op_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/shader_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/filters_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/image_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/text_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/texture_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/precompile_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/font_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/bitmap_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/recorder_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/camera_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/quaternion_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/data_c.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/stroke_c.cpp
)

if (${SKITY_BUILD_INSTALL})
  install(
    DIRECTORY
    "${CMAKE_CURRENT_LIST_DIR}/include/skity_c"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h"
  )

  install(
    TARGETS skity-capi EXPORT skity-capi-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  )

  install(
    EXPORT skity-capi-targets
    FILE skityCapiTargets.cmake
    NAMESPACE skity::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skity
  )
endif()
