# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# skity C API + header-only C++ wrapper example: the same GLFW + OpenGL scene
# as case/c_api, but driven through the RAII layer in skity_hpp/skity.hpp.
# Not-yet-wrapped entry points (shader, recorder, display list) are called
# through the raw C API to show the intended mixed usage during migration.

add_executable(skity-c-api-hpp main.cpp)

target_link_libraries(skity-c-api-hpp PRIVATE skity::capi glfw)

set_target_properties(skity-c-api-hpp PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)
