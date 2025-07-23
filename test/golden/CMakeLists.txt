# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

option(SKITY_GOLDEN_GUI "Option to open GUI for golden test" ON)

if (NOT APPLE)
  message(FATAL_ERROR "Currently golden test only support Metal environment")
endif()

if (NOT ${SKITY_MTL_RENDERER})
  message(FATAL_ERROR "Currently golden test only support Metal renderer")
endif()

set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -fobjc-arc", "-std=c++17")

add_executable(skity_golden_test
  ${CMAKE_CURRENT_LIST_DIR}/golden_test.cc
  ${CMAKE_CURRENT_LIST_DIR}/common/golden_test_check.cc
  ${CMAKE_CURRENT_LIST_DIR}/common/golden_test_check.hpp
  ${CMAKE_CURRENT_LIST_DIR}/common/golden_test_env.cc
  ${CMAKE_CURRENT_LIST_DIR}/common/golden_test_env.hpp
  ${CMAKE_CURRENT_LIST_DIR}/common/golden_texture.cc
  ${CMAKE_CURRENT_LIST_DIR}/common/golden_texture.hpp

  # Metal backend code
  ${CMAKE_CURRENT_LIST_DIR}/common/mtl/golden_test_env_mtl.h
  ${CMAKE_CURRENT_LIST_DIR}/common/mtl/golden_test_env_mtl.mm
  ${CMAKE_CURRENT_LIST_DIR}/common/mtl/golden_texture_mtl.h
  ${CMAKE_CURRENT_LIST_DIR}/common/mtl/golden_texture_mtl.mm
)

target_include_directories(skity_golden_test
  PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(skity_golden_test
  gtest
  skity::skity
  skity::codec
  "-framework Metal"
  "-framework CoreGraphics"
  "-framework ImageIO"
  "-framework CoreServices"
)

target_compile_definitions(skity_golden_test PUBLIC -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)


if (${SKITY_GOLDEN_GUI})
  add_subdirectory(playground)
endif()

set(SKITY_GOLDEN_TEST_DIR ${CMAKE_CURRENT_LIST_DIR})

### add golden test case to skity_golden_test
###
### @param NAME test case name
### @param SOURCES test case source files
### @param INCLUDES test case include directories
### @param LIBS test case link libraries
### @param DEFINES test case compile definitions
###
### @example
### add_golden_test(
###   NAME      <case name>
###   SOURCES   <source files>
###   INCLUDES  <custom include path if needed>
###   LIBS      <other library if needs>
###   DEFINES   <other compile definitions>
### )
###
function(add_golden_test)
  set(options )
  set(oneValueArgs NAME)
  set(multiValueArgs SOURCES INCLUDES LIBS DEFINES)

  cmake_parse_arguments(
    PARSE_ARGV 0
    ARG
    "${options}"
    "${oneValueArgs}"
    "${multiValueArgs}"
  )

  if (NOT ARG_NAME)
    message(FATAL_ERROR "add_golden_test: NAME is required")
  endif()

  message(STATUS "add_golden_test: ${ARG_NAME}")

  if (NOT ARG_SOURCES)
    message(FATAL_ERROR "add_golden_test: SOURCES is required")
  endif()

  add_library(${ARG_NAME} OBJECT
    ${ARG_SOURCES}
  )

  if (ARG_INCLUDES)
    target_include_directories(${ARG_NAME}
      PRIVATE
      ${ARG_INCLUDES}
    )
  endif()

  target_link_libraries(${ARG_NAME} PRIVATE gtest skity::skity)
  target_include_directories(${ARG_NAME} PRIVATE ${SKITY_GOLDEN_TEST_DIR})

  if (ARG_LIBS)
    target_link_libraries(${ARG_NAME}
      ${ARG_LIBS}
    )
  endif()

  target_compile_definitions(${ARG_NAME}
    PRIVATE
    CASE_NAME="${ARG_NAME}"
    CASE_DIR="${CMAKE_CURRENT_LIST_DIR}"
  )

  # append other user defined defines
  if (ARG_DEFINES)
    target_compile_definitions(${ARG_NAME}
      PRIVATE
      ${ARG_DEFINES}
    )
  endif()

  target_link_libraries(skity_golden_test
    $<TARGET_OBJECTS:${ARG_NAME}>
  )

endfunction(add_golden_test)

add_subdirectory(cases)
