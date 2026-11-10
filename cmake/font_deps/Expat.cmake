# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

set(expat_source "${SKITY_ROOT}/third_party/expat/expat")
set(SKITY_EXPAT_INCLUDE_DIR "${expat_source}/lib")
if(NOT EXISTS "${expat_source}/CMakeLists.txt")
  message(FATAL_ERROR
    "Upstream Expat sources are missing at ${expat_source}; sync dependencies or select SKITY_FONT_DEPS_CMAKE_DIR")
endif()

set(EXPAT_SHARED_LIBS OFF)
set(EXPAT_BUILD_TOOLS OFF)
set(EXPAT_BUILD_EXAMPLES OFF)
set(EXPAT_BUILD_TESTS OFF)
set(EXPAT_BUILD_DOCS OFF)
set(EXPAT_BUILD_PKGCONFIG OFF)
set(EXPAT_ENABLE_INSTALL OFF)
set(EXPAT_CHAR_TYPE char)
add_subdirectory("${expat_source}" "${CMAKE_BINARY_DIR}/third_party/expat" EXCLUDE_FROM_ALL)
