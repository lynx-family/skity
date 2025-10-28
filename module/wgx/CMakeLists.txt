# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

option(WGX_GLSL_BACKEND "Generate GLSL shader" ON)
option(WGX_MSL_BACKEND "Generate MSL shader" ON)

add_library(wgsl-cross STATIC
    ${CMAKE_CURRENT_LIST_DIR}/include/wgsl_cross.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/attribute.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/attribute.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/expression.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/expression.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/function.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/function.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/identifier.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/identifier.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/module.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/module.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/node.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/pipeline_stage.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/statement.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/statement.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/type.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/type.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/type_decl.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/type_decl.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/variable.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/variable.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/ast/visitor.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/bind_group.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/function.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/function.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/parser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/parser.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/program.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/scanner.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/scanner.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/token.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/token.h
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/type_definition.cpp
    ${CMAKE_CURRENT_LIST_DIR}/wgsl/type_definition.h
)

if (WGX_GLSL_BACKEND)
    target_compile_definitions(wgsl-cross PRIVATE -DWGX_GLSL=1)
    target_sources(wgsl-cross PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/glsl/ast_printer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/glsl/ast_printer.h
    )
endif()

if (WGX_MSL_BACKEND)
    target_compile_definitions(wgsl-cross PRIVATE -DWGX_MSL=1)
    target_sources(wgsl-cross PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/msl/ast_printer.cpp
        ${CMAKE_CURRENT_LIST_DIR}/msl/ast_printer.h
        ${CMAKE_CURRENT_LIST_DIR}/msl/attribute.cpp
        ${CMAKE_CURRENT_LIST_DIR}/msl/attribute.h
        ${CMAKE_CURRENT_LIST_DIR}/msl/uniform_capture.cpp
        ${CMAKE_CURRENT_LIST_DIR}/msl/uniform_capture.h
    )
endif()

target_include_directories(wgsl-cross PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(wgsl-cross PRIVATE ${CMAKE_CURRENT_LIST_DIR})
target_compile_options(wgsl-cross PRIVATE -fno-rtti)
set_property(TARGET wgsl-cross PROPERTY CXX_STANDARD 17)

# no-exceptions
if (MSVC)
    target_compile_options(wgsl-cross PRIVATE /EHsc)
else()
    target_compile_options(wgsl-cross PRIVATE -fno-exceptions)
endif()

if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(wgsl-cross PRIVATE -fPIC)
endif()

# Link with libcxx for MSVC/clang-cl on Windows when using self libcxx
if (${SKITY_USE_SELF_LIBCXX} AND WIN32)
    target_link_libraries(wgsl-cross PRIVATE third_party__libcxx)
endif()
