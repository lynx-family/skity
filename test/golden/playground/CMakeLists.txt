# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

if (NOT ${SKITY_EXAMPLE})
  # FIXME: if build skity example, the glfw library is add to this cmake project
  add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/glfw third_party/glfw)
endif()


target_sources(skity_golden_test
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/playground.cc
  ${CMAKE_CURRENT_LIST_DIR}/playground.hpp
  ${CMAKE_CURRENT_LIST_DIR}/window.cc
  ${CMAKE_CURRENT_LIST_DIR}/window.hpp

  # Metal backend
  ${CMAKE_CURRENT_LIST_DIR}/mtl/window_mtl.h
  ${CMAKE_CURRENT_LIST_DIR}/mtl/window_mtl.mm
)

target_link_libraries(skity_golden_test
  glfw
  "-framework QuartzCore"
)

target_compile_definitions(
  skity_golden_test
  PUBLIC
  -DSKITY_GOLDEN_GUI=1
)

