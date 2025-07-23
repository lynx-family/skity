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
)

target_compile_definitions(skity-codec PRIVATE -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)

# libpng
ExternalProject_Add(libpng
  SOURCE_DIR ${SKITY_ROOT}/third_party/libpng
  PREFIX ${CMAKE_CURRENT_BINARY_DIR}/third_party/libpng
  CMAKE_ARGS
  -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/third_party/libpng
  -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
  -DPNG_EXECUTABLES=OFF
  -DPNG_SHARED=ON
  -DPNG_STATIC=OFF
  -DPNG_FRAMEWORK=OFF
  -DPNG_TESTS=OFF
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

# codec backends
target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/png_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/png_codec.hpp
)

target_compile_definitions(skity-codec PUBLIC -DSKITY_HAS_PNG=1)

add_dependencies(skity-codec libpng)

target_include_directories(skity-codec PRIVATE ${CMAKE_BINARY_DIR}/third_party/libpng/include)
target_link_directories(skity-codec PUBLIC ${CMAKE_BINARY_DIR}/third_party/libpng/lib)
target_link_libraries(skity-codec PRIVATE png)


target_sources(
  skity-codec
  PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/jpeg_codec.cc
  ${CMAKE_CURRENT_LIST_DIR}/src/codec/jpeg_codec.hpp
)

add_dependencies(skity-codec libjpeg-turbo)

target_include_directories(skity-codec PRIVATE ${CMAKE_BINARY_DIR}/third_party/libjpeg-turbo/include)
target_link_directories(skity-codec PUBLIC ${CMAKE_BINARY_DIR}/third_party/libjpeg-turbo/lib)
target_link_libraries(skity-codec PRIVATE turbojpeg)

target_compile_definitions(skity-codec PUBLIC -DSKITY_HAS_JPEG=1)

