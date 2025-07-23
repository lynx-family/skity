# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(nanovg_frame_example
    frame_example.cc
    main.cc
    perf.cc
    perf.hpp
)
target_include_directories(nanovg_frame_example PUBLIC .)
target_compile_definitions(nanovg_frame_example PRIVATE -DEXAMPLE_IMAGE_ROOT="${CMAKE_SOURCE_DIR}/example/images/")

target_link_libraries(nanovg_frame_example PUBLIC skity::example_common)
