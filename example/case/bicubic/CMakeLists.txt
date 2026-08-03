# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(bicubic_example
    bicubic_example.cc
    bicubic_example.hpp
    main.cc
)

target_compile_definitions(bicubic_example PRIVATE -DEXAMPLE_IMAGE_ROOT="${CMAKE_SOURCE_DIR}/example/images/")

target_compile_options(bicubic_example PRIVATE "-fobjc-arc")
target_link_libraries(bicubic_example PUBLIC skity::example_common)
