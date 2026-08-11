# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# skity C API example: a GLFW + OpenGL window driven entirely through the C ABI.
# glfw is provided by example/common/CMakeLists.txt.

add_executable(skity-c-api main.c)

target_link_libraries(skity-c-api PRIVATE skity::capi glfw)

set_target_properties(skity-c-api PROPERTIES
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
)
