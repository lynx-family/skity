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
    case/draw_canvasmark.cc
    case/draw_canvasmark.hpp
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

# C-API + RAII-wrapper twin of the CanvasMark case. Compiled as its own dylib
# so its <skity/...> includes resolve to the wrapper header tree
# (module/capi/wrapper) instead of the legacy headers the rest of the bench
# compiles against — the two cannot share a translation unit. They also should
# not share a link unit casually: before the wrapper moved into
# `inline namespace skity::raii` its inline skity::Paint / skity::Path members
# mangled to the same names as the legacy inline members, and a STATIC library
# then won symbol resolution against libskity.dylib — legacy TUs silently
# constructed wrapper objects and crashed on the first draw (null vtable in
# HWCanvas::DrawShape). The raii namespace makes the symbols distinct
# (_ZN5skity4raii...), so a static library is safe again; this target stays a
# SHARED library with -fvisibility=hidden (only the extern "C" entries
# exported) as defense in depth for the mixed-API executable.
add_library(skity_bench_canvasmark_capi SHARED
    case/draw_canvasmark_capi.cc
    case/draw_canvasmark_capi.h
    case/canvasmark_scene.hpp
)
target_include_directories(skity_bench_canvasmark_capi
    PRIVATE
    ${CMAKE_SOURCE_DIR}/module/capi/wrapper/include
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(skity_bench_canvasmark_capi
    PRIVATE
    skity::capi
    skity-capi-internal
)
target_compile_options(skity_bench_canvasmark_capi PRIVATE -fno-rtti)
target_compile_options(skity_bench_canvasmark_capi PRIVATE -std=c++17)
target_compile_options(skity_bench_canvasmark_capi PRIVATE -fvisibility=hidden)
# The wrapper headers refuse to compile without SKITY_UNIFIED_LIB (see
# module/capi/wrapper/include/skity/base.hpp). The normal-shape skity-capi
# carries the same C entry points, and the consumer->dylib call pattern the
# benchmark measures is identical, so opting in here is sound.
target_compile_definitions(skity_bench_canvasmark_capi PRIVATE -DSKITY_UNIFIED_LIB=1)
target_compile_definitions(skity_bench_canvasmark_capi PRIVATE -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)

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
# draw_skp bridges the recorder canvas into the C ABI picture playback.
target_link_libraries(skity_hw_render_bench PRIVATE skity-capi-internal)
# Linked last so the executable resolves every legacy inline symbol against
# libskity.dylib before considering the C-API case dylib (defensive; the
# hidden-visibility build already prevents any collision).
target_link_libraries(skity_hw_render_bench PRIVATE skity_bench_canvasmark_capi)

if (SKITY_BENCH_ENABLE_PERFETTO)
    set(PERFETTO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/perfetto")
    add_library(perfetto STATIC "${PERFETTO_SDK_DIR}/perfetto.cc")
    target_compile_definitions(perfetto PRIVATE PERFETTO_SDK_IMPLEMENTATION)
    target_include_directories(perfetto PUBLIC "${PERFETTO_SDK_DIR}")
    target_compile_features(perfetto PRIVATE cxx_std_17)

    target_compile_definitions(skity_hw_render_bench PRIVATE SKITY_BENCH_ENABLE_PERFETTO=1)
    target_link_libraries(skity_hw_render_bench PRIVATE perfetto)
endif()
