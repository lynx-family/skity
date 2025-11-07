# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

set(INSTALL_GTEST OFF CACHE BOOL "disable INSTALL_GTEST")
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/gtest third_party/gtest)

add_subdirectory(ut)

if(${SKITY_SW_RENDERER})
  add_subdirectory(bench)
endif()

if (${SKITY_MTL_BACKEND} AND ${SKITY_CODEC_MODULE})
  add_subdirectory(golden)
endif()

