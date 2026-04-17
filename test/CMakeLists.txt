# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

set(INSTALL_GTEST OFF CACHE BOOL "disable INSTALL_GTEST")
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/gtest third_party/gtest)

if(${SKITY_TEST_UT})
  add_subdirectory(ut)
endif()

if(${SKITY_TEST_BENCH} AND ${SKITY_SW_RENDERER})
  add_subdirectory(bench)
endif()

if(${SKITY_TEST_GOLDEN} AND ${SKITY_MTL_BACKEND} AND ${SKITY_CODEC_MODULE})
  add_subdirectory(golden)
endif()
