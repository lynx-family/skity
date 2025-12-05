# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "disable BENCHMARK_ENABLE_INSTALL")
set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "disable BENCHMARK_INSTALL_DOCS")
add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/google_benchmark third_party/google_benchmark)
add_subdirectory(micro)

if (NOT APPLE)
    return()
endif()

add_executable(skity_hw_render_bench
    hw_benchmarks.cc
    case/draw_circle.cc
    case/draw_circle.hpp
    case/draw_skp.cc
    case/draw_skp.hpp
)

target_sources(skity_hw_render_bench PRIVATE 
    common/bench_target.cc
    common/bench_target.hpp
    common/bench_context.cc
    common/bench_context.hpp
)

target_compile_options(skity_hw_render_bench PRIVATE "-fobjc-arc")

if (${SKITY_MTL_BACKEND})
  target_compile_definitions(skity_hw_render_bench PRIVATE -DSKITY_BENCH_MTL_BACKEND=1)
  target_sources(skity_hw_render_bench PRIVATE 
    common/bench_target_mtl.h
    common/bench_target_mtl.mm
    common/bench_context_mtl.h
    common/bench_context_mtl.mm
    common/bench_command_queue_proxy.h
    common/bench_command_queue_proxy.mm
    common/bench_gpu_time_tracer.hpp
    common/bench_gpu_time_tracer.cc
  )
  target_link_libraries(skity_hw_render_bench PRIVATE "-framework Metal")
endif()

if (${SKITY_GL_BACKEND})
    target_compile_definitions(skity_hw_render_bench PRIVATE -DSKITY_BENCH_GL_BACKEND=1)
    target_sources(skity_hw_render_bench PRIVATE 
        common/bench_target_gl.cc
        common/bench_target_gl.hpp
        common/bench_context_gl.cc
        common/bench_context_gl.hpp
        common/bench_gl_context_mac.h
        common/bench_gl_context_mac.mm
        common/bench_gl_context.hpp
    )
    target_link_libraries(skity_hw_render_bench PRIVATE "-framework OpenGL")
endif()

target_include_directories(skity_hw_render_bench
    PUBLIC
    ${CMAKE_SOURCE_DIR}
)

target_compile_options(skity_hw_render_bench PUBLIC -fno-rtti)
target_compile_options(skity_hw_render_bench PUBLIC -std=c++17)
target_compile_definitions(skity_hw_render_bench PUBLIC -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)
target_compile_definitions(skity_hw_render_bench PRIVATE RESOURCES_DIR="${SKITY_ROOT}/resources")

target_link_libraries(skity_hw_render_bench
    PUBLIC
    skity::codec
    skity::skity
    skity::io
    benchmark::benchmark
)
target_link_libraries(skity_hw_render_bench
    PRIVATE "-framework Carbon"
            "-framework AppKit")

target_link_libraries(skity_hw_render_bench PRIVATE glm::glm-header-only)
