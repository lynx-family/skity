# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# The unified-shape C++ consumer example: the basic demo written against the
# header-only RAII wrapper (the skity/ wrapper headers, legacy-identical paths) over the C ABI. GLFW + OpenGL,
# configured only in the unified shape (the wrapper refuses to compile
# without SKITY_UNIFIED_LIB, which the skity target exports there).

add_executable(skity-c-api-hpp main.cc)

target_link_libraries(skity-c-api-hpp PRIVATE skity::capi skity::codec glfw)

# skity-codec lists its C-header root PRIVATE; consumers add it themselves.
target_include_directories(skity-c-api-hpp PRIVATE
    ${CMAKE_SOURCE_DIR}/module/codec/include)

target_compile_definitions(skity-c-api-hpp PRIVATE
    SKITY_FONT_DIR="${CMAKE_SOURCE_DIR}/test/")
