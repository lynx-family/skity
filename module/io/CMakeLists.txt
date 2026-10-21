# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_library(skity-io STATIC
    # internal engine headers (the public C++ surface is gone; the
    # consumable surface is the C ABI under include/skity_c/)
    src/picture.hpp
    src/stream/stream.hpp
    src/utils/parse_path.hpp

    # source files
    src/io/flat/blender_flat.cc
    src/io/flat/blender_flat.hpp
    src/io/flat/blob_flat.cc
    src/io/flat/color_filter_flat.cc
    src/io/flat/font_desc_flat.cc
    src/io/flat/font_desc_flat.hpp
    src/io/flat/font_flat.cc
    src/io/flat/image_filter_flat.cc
    src/io/flat/local_matrix_flat.cc
    src/io/flat/local_matrix_flat.hpp
    src/io/flat/mask_filter_flat.cc
    src/io/flat/matrix_flat.cc
    src/io/flat/paint_flat.cc
    src/io/flat/path_flat.cc
    src/io/flat/path_effect_flat.cc
    src/io/flat/rrect_flat.cc
    src/io/flat/shader_flat.cc
    src/io/flat/vertices_flat.cc
    src/io/read/read_typeface.cc
    src/io/read/read_typeface.hpp
    src/io/memory_read.cc
    src/io/memory_read.hpp
    src/io/memory_writer.cc
    src/io/memory_writer.hpp
    src/record/draw_type.hpp
    src/record/record_playback.cc
    src/record/record_playback.hpp
    src/stream/file_read_stream.cc
    src/stream/file_read_stream.hpp
    src/stream/file_write_stream.cc
    src/stream/file_write_stream.hpp
    src/stream/stream.cc
    src/utils/parse_path.cc
    src/picture_priv.hpp
    src/picture.cc
)


add_library(skity::io ALIAS skity-io)

target_include_directories(skity-io
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

target_include_directories(skity-io
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    # resolves "src/handle.hpp" (module/capi/src/handle.hpp) for
    # core_bridge.hpp; comes after the module root on purpose so io's own
    # "src/..." includes keep resolving against module/io first
    ${SKITY_ROOT}/module/capi
    # capi paths must also be listed here directly: INTERFACE-library usage
    # requirements do not survive cmake_to_gn, so "handle.hpp" and the shared
    # skity_c headers would be unresolvable in the GN build
    ${SKITY_ROOT}/module/capi/src
    ${SKITY_ROOT}/module/capi/include
)

# C ABI surface (shipped with the module): streams + picture
target_sources(skity-io PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include/skity_c/skity_stream.h
    ${CMAKE_CURRENT_LIST_DIR}/include/skity_c/skity_picture.h
    ${CMAKE_CURRENT_LIST_DIR}/include/skity_c/skity_parse_path.h
    ${CMAKE_CURRENT_LIST_DIR}/src/capi/io_c.cpp
)

target_link_libraries(skity-io
    PUBLIC
    skity::skity
    skity::codec
)

# the module's own C ABI surface shares the capi handle protocol
target_link_libraries(skity-io PRIVATE skity-capi-internal)
# The module internals call the core C ABI (see src/io/core_bridge.hpp);
# those entry points live in the standalone skity-capi library.
target_link_libraries(skity-io PRIVATE skity::capi)
# core_bridge.hpp computes rotation matrices with the same glm the core
# uses, keeping the serialized bytes bit-identical.
target_include_directories(skity-io PRIVATE ${SKITY_ROOT}/third_party/glm)

set_property(TARGET skity-io PROPERTY CXX_STANDARD 17)
set_property(TARGET skity-io PROPERTY CXX_STANDARD_REQUIRED ON)

# no-exceptions
if (MSVC)
    target_compile_options(skity-io PRIVATE /EHsc)
else()
    target_compile_options(skity-io PRIVATE -fno-exceptions -fno-rtti)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(skity-io PRIVATE -fPIC)
endif()

