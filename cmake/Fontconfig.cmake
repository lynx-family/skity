# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include_guard(GLOBAL)

set(SKITY_FONT_DEPS_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}/font_deps" CACHE PATH
    "Directory containing Expat.cmake and Fontconfig.cmake dependency adapters")
include("${CMAKE_CURRENT_LIST_DIR}/font_deps/FontconfigCommon.cmake")

# Keep provider options and dependency configuration local to this function.
function(skity_build_fontconfig)
  if(CMAKE_VERSION VERSION_LESS 3.17)
    message(FATAL_ERROR "Bundled Expat requires CMake 3.17 or newer")
  endif()
  get_filename_component(font_deps_dir "${SKITY_FONT_DEPS_CMAKE_DIR}"
    ABSOLUTE BASE_DIR "${CMAKE_SOURCE_DIR}")
  foreach(dependency Expat Fontconfig)
    if(NOT EXISTS "${font_deps_dir}/${dependency}.cmake")
      message(FATAL_ERROR
        "SKITY_FONT_DEPS_CMAKE_DIR must contain Expat.cmake and Fontconfig.cmake: ${font_deps_dir}")
    endif()
  endforeach()
  message(STATUS "Skity font dependency scripts: ${font_deps_dir}")

  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  set_target_properties(freetype2 PROPERTIES POSITION_INDEPENDENT_CODE ON)
  include("${font_deps_dir}/Expat.cmake")
  if(NOT TARGET expat OR NOT EXISTS "${SKITY_EXPAT_INCLUDE_DIR}/expat.h")
    message(FATAL_ERROR
      "Expat.cmake must define the expat target and SKITY_EXPAT_INCLUDE_DIR")
  endif()
  include("${font_deps_dir}/Fontconfig.cmake")
  if(NOT TARGET skity_fontconfig)
    message(FATAL_ERROR "Fontconfig.cmake must define the skity_fontconfig target")
  endif()

  # Keep bundled font libraries private when linking the Skity shared library.
  target_link_options(skity PRIVATE
    "LINKER:--exclude-libs,libfontconfig.a:$<TARGET_FILE_NAME:expat>:$<TARGET_FILE_NAME:freetype2>")
endfunction()

skity_build_fontconfig()
