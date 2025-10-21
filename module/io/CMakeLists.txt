# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_library(skity-io STATIC
    # header files
    include/skity/io/picture.hpp
    include/skity/io/stream.hpp

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
)

target_link_libraries(skity-io
    PUBLIC
    skity::skity
    skity::codec
)

set_property(TARGET skity-io PROPERTY CXX_STANDARD 17)
set_property(TARGET skity-io PROPERTY CXX_STANDARD_REQUIRED ON)

# no-exceptions
if (MSVC)
    target_compile_options(skity-io PRIVATE /EHsc)
else()
    target_compile_options(skity-io PRIVATE -fno-exceptions)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(skity-io PRIVATE -fPIC)
endif()

