# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include(GoogleTest)

enable_testing()

# Test case list
add_executable(skity_unit_test
    effect/color_filter_test.cc
    effect/image_filter_test.cc
    geometry/geometry_test.cc
    geometry/math_test.cc
    geometry/matrix_test.cc
    geometry/quaternion_test.cc
    geometry/rect_test.cc
    geometry/rrect_test.cc
    geometry/scalar_test.cc
    geometry/vector_test.cc
    graphic/bitmap_test.cc
    graphic/color_test.cc
    graphic/path_measure_test.cc
    graphic/path_test.cc
    io/pixmap_test.cc
    render/canvas_state_test.cc
    render/resource_cache_test.cc
    recorder/display_list_test.cc
    text/text_run_test.cc
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

if (${SKITY_CODEC_MODULE})
    target_sources(skity_unit_test
        PUBLIC
        codec/jpeg_codec_test.cc
        codec/png_codec_test.cc
    )

    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_PNG_FILE="${CMAKE_SOURCE_DIR}/example/images/firefox_64.png")
    target_compile_definitions(skity_unit_test PRIVATE -DSKITY_TEST_JPEG_FILE="${CMAKE_SOURCE_DIR}/example/images/image1.jpg")

    target_link_libraries(skity_unit_test PUBLIC skity::codec)
endif()

if (${SKITY_TEST_COVERAGE} AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Coverage flags
    target_compile_options(skity PRIVATE --coverage -O0 -g)
    target_link_options(skity PRIVATE --coverage)

    target_compile_options(wgsl-cross PRIVATE --coverage -O0 -g)
    target_link_options(wgsl-cross PRIVATE --coverage)

    if (${SKITY_CODEC_MODULE})
        target_compile_options(skity-codec PRIVATE --coverage -O0 -g)
        target_link_options(skity-codec PRIVATE --coverage)
    endif()

    target_compile_options(skity_unit_test PRIVATE --coverage -O0 -g)
    target_link_options(skity_unit_test PRIVATE --coverage)
endif()

target_link_libraries(skity_unit_test PRIVATE glm::glm-header-only)

gtest_add_tests(skity_unit_test "" AUTO)
