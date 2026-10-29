# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# Vulkan-style C ABI wrapper around libskity. This is a thin forwarding layer
# that links the core skity library and re-exports a stable C surface. See
# module/capi/C_API_DESIGN.md for the full design.
#
# Two mutually exclusive shapes (SKITY_UNIFIED_LIB):
# - OFF (default): the standalone skity-capi shared library below.
# - ON: the sources compile straight into libskity, which hides the legacy
#   C++ API and exports only the skity_* C ABI; the header-only C++ wrapper
#   under wrapper/include/skity/ becomes the C++ surface (its paths spell
#   exactly like the legacy headers) and skity::capi
#   is an alias of skity so consumers keep their link lines.
#
# Two include roots, one per surface: include/ carries the C ABI (skity/c)
# plus the C++ bridge header (skity/hpp/bridge.hpp) — everything valid in
# BOTH shapes — while wrapper/include/ carries the SKITY_UNIFIED_LIB-only
# RAII layer (skity/**, spelled EXACTLY like the legacy headers so consumer
# include lines never change between shapes; the roots are never both
# public, the build shape decides which tree <skity/...> resolves to).
# Keeping them apart lets every consumer of the
# include root (make install rules, the iOS framework, the Android prefab)
# pick up the normal-shape surface without per-file filtering.

# Vulkan-only wrapper sources depend on the vendored Vulkan headers, so they
# are only compiled when the Vulkan backend is enabled.
set(SKITY_CAPI_VULKAN_SOURCES)
if (${SKITY_VK_BACKEND})
  list(APPEND SKITY_CAPI_VULKAN_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/presenter_vk.cpp
  )
endif()

# The Metal side (typed entry points + glue) needs Objective-C++ (Metal /
# CAMetalLayer headers), so it is only compiled when the Metal backend is
# enabled; context_c.cpp carries portable SKITY_ERROR_NOT_SUPPORTED stubs
# without it.
set(SKITY_CAPI_METAL_SOURCES)
if (${SKITY_MTL_BACKEND})
  list(APPEND SKITY_CAPI_METAL_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/src/mtl_glue.mm
  )
endif()

set(SKITY_CAPI_SOURCES
  ${CMAKE_CURRENT_LIST_DIR}/common/handle.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/context_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/surface_c.cpp
  ${SKITY_CAPI_VULKAN_SOURCES}
  ${SKITY_CAPI_METAL_SOURCES}
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
  ${CMAKE_CURRENT_LIST_DIR}/src/glyph_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/semaphore_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/render_target_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/bitmap_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/recorder_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/camera_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/quaternion_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/data_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/stroke_c.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/bridge_c.cpp
)

if (${SKITY_UNIFIED_LIB})
  # ---- unified shape: the C ABI lives inside libskity itself ----

  target_sources(skity PRIVATE ${SKITY_CAPI_SOURCES})

  # the C ABI sources reach the handle protocol (common/handle.hpp) the same
  # way the standalone skity-capi does
  target_include_directories(skity PRIVATE ${CMAKE_CURRENT_LIST_DIR}/common)

  # the skity/c headers and the wrapper root become part of skity's public
  # interface (both roots install into <includedir>/skity/...)
  target_include_directories(skity
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
      $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/wrapper/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )

  # The backend guards (SKITY_OPENGL / SKITY_VULKAN PUBLIC, SKITY_METAL
  # PRIVATE) and the vendored Vulkan headers are already on the skity target.
  # -fno-strict-aliasing is required because the wrapper reinterprets the
  # binary-compatible POD structs (skity_rect/matrix/...) as their skity::
  # counterparts across the boundary.
  if (NOT MSVC)
    set_source_files_properties(${SKITY_CAPI_SOURCES} PROPERTIES
      COMPILE_OPTIONS "-fno-strict-aliasing")
  endif()

  # Hide the legacy C++ API (SKITY_API neutralized + export table restricted
  # to skity_*) and tell wrapper consumers this is the unified shape they may
  # compile the wrapper headers against.
  target_compile_definitions(skity PRIVATE SKITY_HIDE_CXX_API=1)
  target_compile_definitions(skity INTERFACE SKITY_UNIFIED_LIB=1)
  skity_export_c_abi_only(skity)

  # consumers keep the skity::capi link line
  add_library(skity::capi ALIAS skity)
else()
  # ---- normal shape: the standalone skity-capi library ----

  add_library(skity-capi SHARED)
  add_library(skity::capi ALIAS skity-capi)

  target_include_directories(skity-capi
    PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
  )
  # module root (for "src/..." includes) and skity public headers
  target_include_directories(skity-capi PRIVATE ${CMAKE_CURRENT_LIST_DIR})
  # the handle protocol (common/handle.hpp) shared with the module-owned C
  # surfaces; see skity-capi-internal in the top-level CMakeLists.txt
  target_include_directories(skity-capi PRIVATE ${CMAKE_CURRENT_LIST_DIR}/common)
  target_include_directories(skity-capi PRIVATE ${SKITY_ROOT}/include)

  target_link_libraries(skity-capi PUBLIC skity)

  target_compile_features(skity-capi PRIVATE cxx_std_17)

  # Match the core library's flags. -fno-strict-aliasing is required because
  # the wrapper reinterprets the binary-compatible POD structs
  # (skity_rect/matrix/...) as their skity:: C++ counterparts across the
  # boundary.
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
    # skity keeps this include path private, so its C API target must add the
    # vendored Vulkan headers for its own wrapper sources.
    target_include_directories(skity-capi PRIVATE
      ${SKITY_ROOT}/third_party/Vulkan-Headers/include
    )
  endif()

  if (${SKITY_MTL_BACKEND})
    target_compile_definitions(skity-capi PRIVATE -DSKITY_METAL)
  endif()

  # Match the core library's LTO setting (see the skity target in the root
  # CMakeLists.txt). On Android release builds skity objects are compiled with
  # -flto, so this wrapper must also link with -flto or strict linkers reject
  # the LTO bitcode pulled in from libskity.so. The other size flags from the
  # core target (--exclude-libs / --gc-sections / --icf) are not needed here:
  # they are size optimizations and capi is a thin forwarding layer.
  if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if (ANDROID)
      target_link_options(skity-capi PRIVATE "-flto")
    endif()
  endif()

  # Android 16 KiB page-size alignment (arm64 / x86_64), required for the .so
  # to load on Android 15+ devices with 16 KiB pages. Mirrors the option
  # applied to the core skity target in cmake/Platform.cmake. Note that only
  # max-page-size is used (see the comment there): LLD 9 pads PT_GNU_RELRO
  # .memsz up to common-page-size but does not extend the covering RW
  # PT_LOAD, which made dlopen on 4 KiB-page devices fail for this thin
  # RELRO-heavy shim.
  if (ANDROID)
    if (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" OR CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
      target_link_options(skity-capi PRIVATE
        "-Wl,-z,max-page-size=16384"
      )
    endif()
  endif()

  target_sources(skity-capi PRIVATE ${SKITY_CAPI_SOURCES})
endif()

if (${SKITY_BUILD_INSTALL})
  # include/ holds exactly the both-shapes surface — the skity/c C ABI plus
  # the skity/hpp bridge header — so one rule covers the normal shape
  install(
    DIRECTORY
    "${CMAKE_CURRENT_LIST_DIR}/include/skity"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
  )

  # unified: the header-only wrapper joins as the C++ surface (its plain
  # skity:: classes are only sound against the C-ABI-only library)
  if (${SKITY_UNIFIED_LIB})
    install(
      DIRECTORY
      "${CMAKE_CURRENT_LIST_DIR}/wrapper/include/skity"
      DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
      FILES_MATCHING PATTERN "*.hpp"
    )
  endif()

  if (NOT ${SKITY_UNIFIED_LIB})
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
endif()
