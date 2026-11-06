# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.
cmake_minimum_required(VERSION 3.15)
project(SkityInstalledCppConsumer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(skity CONFIG REQUIRED)
if(NOT EXISTS "${GOOGLETEST_SOURCE_DIR}/CMakeLists.txt")
  message(FATAL_ERROR "Set GOOGLETEST_SOURCE_DIR to synced third_party/gtest")
endif()
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
add_subdirectory("${GOOGLETEST_SOURCE_DIR}" googletest)
add_executable(skity_unit_test public_product_test.cc)
set_target_properties(skity_unit_test PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/test/ut")
target_link_libraries(skity_unit_test PRIVATE skity::skity skity::codec gtest_main)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/imported-includes.txt"
  CONTENT "$<TARGET_PROPERTY:skity::skity,INTERFACE_INCLUDE_DIRECTORIES>\n$<TARGET_PROPERTY:skity::codec,INTERFACE_INCLUDE_DIRECTORIES>\n")
