# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

option(WGX_GLSL_BACKEND "Generate GLSL shader" ON)
option(WGX_MSL_BACKEND "Generate MSL shader" ON)

add_library(wgsl-cross STATIC
    include/wgsl_cross.h
    wgsl/ast/attribute.cpp
    wgsl/ast/attribute.h
    wgsl/ast/expression.cpp
    wgsl/ast/expression.h
    wgsl/ast/function.cpp
    wgsl/ast/function.h
    wgsl/ast/identifier.cpp
    wgsl/ast/identifier.h
    wgsl/ast/module.cpp
    wgsl/ast/module.h
    wgsl/ast/node.h
    wgsl/ast/pipeline_stage.h
    wgsl/ast/statement.cpp
    wgsl/ast/statement.h
    wgsl/ast/type.cpp
    wgsl/ast/type.h
    wgsl/ast/type_decl.cpp
    wgsl/ast/type_decl.h
    wgsl/ast/variable.cpp
    wgsl/ast/variable.h
    wgsl/ast/visitor.h
    wgsl/bind_group.cpp
    wgsl/function.cpp
    wgsl/function.h
    wgsl/parser.cpp
    wgsl/parser.h
    wgsl/program.cpp
    wgsl/scanner.cpp
    wgsl/scanner.h
    wgsl/token.cpp
    wgsl/token.h
    wgsl/type_definition.cpp
    wgsl/type_definition.h
)

if (WGX_GLSL_BACKEND)
    target_compile_definitions(wgsl-cross PRIVATE -DWGX_GLSL=1)
    target_sources(wgsl-cross PRIVATE
        glsl/ast_printer.cpp
        glsl/ast_printer.h
    )
endif()

if (WGX_MSL_BACKEND)
    target_compile_definitions(wgsl-cross PRIVATE -DWGX_MSL=1)
    target_sources(wgsl-cross PRIVATE
        msl/ast_printer.cpp
        msl/ast_printer.h
        msl/attribute.cpp
        msl/attribute.h
        msl/uniform_capture.cpp
        msl/uniform_capture.h
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
