# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(skity_micro_bench
    array_list_benchmarks.cc
    hw_path_raster_benchmarks.cc
    matrix_benchmarks.cc
    micro_bench_main.cc
    sw_benchmarks.cc
    ${CMAKE_SOURCE_DIR}/example/case/basic/example.cc
    ${CMAKE_SOURCE_DIR}/example/case/basic/example.hpp
)

target_include_directories(skity_micro_bench
    PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/example
)

target_compile_options(skity_micro_bench PUBLIC -fno-rtti)
target_compile_options(skity_micro_bench PUBLIC -std=c++17)
target_compile_definitions(skity_micro_bench PUBLIC -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)

target_link_libraries(skity_micro_bench
    PUBLIC
    skity::skity
    benchmark::benchmark
)

target_link_libraries(skity_micro_bench PRIVATE glm::glm-header-only)
