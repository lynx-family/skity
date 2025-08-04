# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include(GoogleTest)

enable_testing()

# Test case list
add_executable(skity_unit_test
    effect/image_filter_test.cc
    geometry/geometry_test.cc
    geometry/math_test.cc
    geometry/matrix_test.cc
    geometry/quaternion_test.cc
    geometry/rect_test.cc
    geometry/vector_test.cc
    graphic/path_measure_test.cc
    graphic/path_test.cc
    render/canvas_state_test.cc
    render/resource_cache_test.cc
    recorder/display_list_test.cc
    text/text_test.cc
    utils/arena_allocator_test.cc
    utils/array_list_test.cc
)

target_include_directories(skity_unit_test
    PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(skity_unit_test
    PUBLIC
    skity::skity
    gmock_main
)

target_link_libraries(skity_unit_test PRIVATE glm::glm-header-only)

gtest_add_tests(skity_unit_test "" AUTO)
