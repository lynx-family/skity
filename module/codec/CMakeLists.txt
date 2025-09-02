# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

include(ExternalProject)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS ON)

add_library(skity-codec STATIC)
add_library(skity::codec ALIAS skity-codec)

target_include_directories(skity-codec PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(skity-codec PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(skity-codec PRIVATE ${SKITY_ROOT}/include)

target_include_directories(
  skity-codec
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include/skity/codec/codec.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/codec_priv.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/codec_priv.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/multi_frame_decoder.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/data_stream.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/data_stream.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/gif_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/gif_codec.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/webp_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/webp_codec.hpp
)

target_compile_definitions(skity-codec PRIVATE -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)

# wuffs
target_include_directories(skity-codec PRIVATE ${CMAKE_SOURCE_DIR}/third_party/wuffs/release/c)

target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/wuffs/wuffs_codec_frame.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/wuffs/wuffs_codec_frame.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/wuffs/wuffs_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/wuffs/wuffs_codec.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/wuffs/wuffs_module.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/wuffs/wuffs_module.hpp
)

# libpng
ExternalProject_Add(libpng
  SOURCE_DIR ${SKITY_ROOT}/third_party/libpng
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/third_party/libpng
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/third_party/libpng
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DPNG_EXECUTABLES=OFF
  -DPNG_SHARED=OFF
  -DPNG_STATIC=ON
  -DPNG_FRAMEWORK=OFF
  -DPNG_TESTS=OFF
  -DZLIB_ROOT=${CMAKE_BINARY_DIR}/third_party/zlib
  -DCMAKE_INSTALL_MESSAGE=NEVER
  -DCMAKE_MESSAGE_LOG_LEVEL=ERROR
)

# libjpeg-turbo
ExternalProject_Add(libjpeg-turbo
  SOURCE_DIR ${SKITY_ROOT}/third_party/libjpeg-turbo
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/third_party/libjpeg-turbo
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/third_party/libjpeg-turbo
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DENABLE_SHARED=OFF
  -DCMAKE_INSTALL_MESSAGE=NEVER
  -DCMAKE_MESSAGE_LOG_LEVEL=ERROR
)

# libwebp
ExternalProject_Add(libwebp
  SOURCE_DIR ${SKITY_ROOT}/third_party/libwebp
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/third_party/libwebp
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/third_party/libwebp
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DENABLE_SHARED=OFF
  -DBUILD_SHARED_LIBS=OFF
  # disable webp command line tools
  -DWEBP_BUILD_CWEBP=OFF
  -DWEBP_BUILD_DWEBP=OFF
  -DWEBP_BUILD_GIF2WEBP=OFF
  -DWEBP_BUILD_IMG2WEBP=OFF
  -DWEBP_BUILD_VWEBP=OFF
  -DWEBP_BUILD_WEBPINFO=OFF
  -DWEBP_BUILD_WEBPMUX=OFF
  -DCMAKE_INSTALL_MESSAGE=NEVER
  -DCMAKE_MESSAGE_LOG_LEVEL=ERROR
)

# codec backends
target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/png_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/png_codec.hpp
)

add_dependencies(libpng skity)
add_dependencies(skity-codec libpng)

target_include_directories(skity-codec PRIVATE ${CMAKE_BINARY_DIR}/third_party/libpng/include)
target_link_directories(skity-codec PUBLIC ${CMAKE_BINARY_DIR}/third_party/libpng/lib)

if (WIN32)
  target_link_directories(skity-codec PUBLIC ${CMAKE_BINARY_DIR}/third_party/zlib/lib)
  # FIXME: msvc generate different library name
  if (NOT CMAKE_BUILD_TYPE OR CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_link_libraries(skity-codec PRIVATE libpng16_staticd zlibstaticd)
  else()
    target_link_libraries(skity-codec PRIVATE libpng16_static zlibstatic)
  endif()
else()
  target_link_libraries(skity-codec PRIVATE png)
  if (${USE_THIRD_PARTY_ZLIB})
    target_link_libraries(skity-codec PRIVATE ${CMAKE_BINARY_DIR}/third_party/zlib/lib/libz.a)
  else()
    target_link_libraries(skity-codec PRIVATE z)
  endif()
endif()

target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/jpeg_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/jpeg_codec.hpp
)

add_dependencies(skity-codec libjpeg-turbo)

target_include_directories(skity-codec PRIVATE ${CMAKE_BINARY_DIR}/third_party/libjpeg-turbo/include)
target_link_directories(skity-codec PUBLIC ${CMAKE_BINARY_DIR}/third_party/libjpeg-turbo/lib)

if (WIN32)
  target_link_libraries(skity-codec PRIVATE turbojpeg-static)
else()
  target_link_libraries(skity-codec PRIVATE turbojpeg)
endif()

add_dependencies(skity-codec libwebp)

target_include_directories(skity-codec PRIVATE ${CMAKE_BINARY_DIR}/third_party/libwebp/include)
target_link_directories(skity-codec PUBLIC ${CMAKE_BINARY_DIR}/third_party/libwebp/lib)


target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/webp/webp_codec_frame.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/webp/webp_codec_frame.hpp
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/webp/webp_decoder.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/webp/webp_decoder.hpp
)

if (WIN32)
  # This link config may not work if using llvm clang under minGW or msys2
  # FIXME: use -llibwebp to solve name conflict with ExternalProject_Add(libwebp ...)
  target_link_libraries(skity-codec PRIVATE libsharpyuv -llibwebp libwebpdecoder libwebpdemux libwebpmux)
else()
  target_link_libraries(skity-codec PRIVATE sharpyuv webp webpdecoder webpdemux webpmux)
endif()
