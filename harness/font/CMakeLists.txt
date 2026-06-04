# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

if(NOT TARGET jsoncpp_static AND NOT TARGET jsoncpp_lib)
  set(JSONCPP_WITH_TESTS OFF CACHE BOOL "disable jsoncpp tests" FORCE)
  add_subdirectory(
    ${CMAKE_SOURCE_DIR}/third_party/jsoncpp
    ${CMAKE_CURRENT_BINARY_DIR}/third_party/jsoncpp
  )
endif()

if(TARGET jsoncpp_static)
  set(SKITY_FONT_HARNESS_JSONCPP_TARGET jsoncpp_static)
elseif(TARGET jsoncpp_lib)
  set(SKITY_FONT_HARNESS_JSONCPP_TARGET jsoncpp_lib)
else()
  message(FATAL_ERROR "jsoncpp target is required for skity_font_harness")
endif()

add_library(skity_font_harness STATIC
  ${CMAKE_CURRENT_LIST_DIR}/artifact/artifact_validator.cc
  ${CMAKE_CURRENT_LIST_DIR}/artifact/artifact_writer.cc
  ${CMAKE_CURRENT_LIST_DIR}/artifact/json_io.cc
  ${CMAKE_CURRENT_LIST_DIR}/case/case_document.cc
  ${CMAKE_CURRENT_LIST_DIR}/case/manifest_document.cc
  ${CMAKE_CURRENT_LIST_DIR}/case/repo_uri.cc
  ${CMAKE_CURRENT_LIST_DIR}/case/validation.cc
  ${CMAKE_CURRENT_LIST_DIR}/compare/compare_engine.cc
  ${CMAKE_CURRENT_LIST_DIR}/compare/path/path_normalizer.cc
  ${CMAKE_CURRENT_LIST_DIR}/platform/coretext/env_info.cc
  ${CMAKE_CURRENT_LIST_DIR}/probe/font_manager_probe.cc
  ${CMAKE_CURRENT_LIST_DIR}/probe/glyph_path_probe.cc
  ${CMAKE_CURRENT_LIST_DIR}/probe/metrics_probe.cc
  ${CMAKE_CURRENT_LIST_DIR}/probe/typeface_probe.cc
  ${CMAKE_SOURCE_DIR}/src/render/text/text_transform.cc
  ${CMAKE_SOURCE_DIR}/src/base/hash.cc
  ${CMAKE_SOURCE_DIR}/src/text/scaler_context.cc
  ${CMAKE_SOURCE_DIR}/src/text/scaler_context_desc.cc
)

set(SKITY_FONT_HARNESS_HAS_CORETEXT 0)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND SKITY_CT_FONT)
  set(SKITY_FONT_HARNESS_HAS_CORETEXT 1)
endif()

target_include_directories(
  skity_font_harness
  PUBLIC
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_features(skity_font_harness PUBLIC cxx_std_17)
target_compile_definitions(
  skity_font_harness
  PUBLIC
  SKITY_FONT_HARNESS_REPO_ROOT="${CMAKE_SOURCE_DIR}"
  SKITY_FONT_HARNESS_CMAKE_SYSTEM_NAME="${CMAKE_SYSTEM_NAME}"
  SKITY_FONT_HARNESS_SKITY_CT_FONT=$<BOOL:${SKITY_CT_FONT}>
  SKITY_FONT_HARNESS_ENABLE_FONT_HARNESS=$<BOOL:${SKITY_ENABLE_FONT_HARNESS}>
  SKITY_FONT_HARNESS_HAS_CORETEXT=${SKITY_FONT_HARNESS_HAS_CORETEXT}
)
target_link_libraries(
  skity_font_harness
  PUBLIC
  skity::skity
  PRIVATE
  glm::glm-header-only
  ${SKITY_FONT_HARNESS_JSONCPP_TARGET}
)

if(SKITY_FONT_HARNESS_HAS_CORETEXT)
  target_link_libraries(
    skity_font_harness
    PRIVATE
    "-framework CoreText"
    "-framework CoreFoundation"
  )
endif()

add_executable(skity-font
  ${CMAKE_CURRENT_LIST_DIR}/cli/skity-font/main.cc
)

target_link_libraries(skity-font PRIVATE skity_font_harness)
target_compile_features(skity-font PRIVATE cxx_std_17)

if(${SKITY_TEST})
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  enable_testing()
  add_test(
    NAME font_harness_cli_smoke
    COMMAND
      ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_LIST_DIR}/tests/smoke/skity_font_cli_smoke.py
      $<TARGET_FILE:skity-font>
  )
  set_tests_properties(font_harness_cli_smoke PROPERTIES LABELS font_harness)
endif()
