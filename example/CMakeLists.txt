# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

project(skity_example)

if(NOT ${SKITY_CODEC_MODULE})
  message(FATAL_ERROR "example code needs codec module")
endif()

add_definitions(-DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)

add_subdirectory(common)

# align output directory
if (MSVC)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
else()
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/example/case)
endif()

add_subdirectory(case/animated_image)
add_subdirectory(case/basic)
add_subdirectory(case/blend)
add_subdirectory(case/clip)
add_subdirectory(case/color_filter)
add_subdirectory(case/complex_layer)
add_subdirectory(case/contour_aa)
add_subdirectory(case/fake_3d)
add_subdirectory(case/gradient)
add_subdirectory(case/image)
add_subdirectory(case/image_filter)
add_subdirectory(case/lock_canvas)
add_subdirectory(case/mask_filter)
add_subdirectory(case/nanovg_frame)
add_subdirectory(case/pathop)
add_subdirectory(case/recorder)
add_subdirectory(case/render_target)
add_subdirectory(case/save_layer)
add_subdirectory(case/shape)
add_subdirectory(case/text)
