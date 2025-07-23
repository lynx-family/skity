# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(image_filter_example
    image_filter_example.cc
    image_filter_example.hpp
    main.cc
)

target_link_libraries(image_filter_example PUBLIC skity::example_common)

target_compile_definitions(image_filter_example PRIVATE -DEXAMPLE_IMAGE_ROOT="${CMAKE_SOURCE_DIR}/example/images/")
