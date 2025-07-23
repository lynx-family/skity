# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_library(basic_example_shared STATIC example.cc example.hpp)
target_link_libraries(basic_example_shared PUBLIC skity::skity)
target_include_directories(basic_example_shared PUBLIC ${CMAKE_SOURCE_DIR}/example)

add_executable(basic_example
    main.cc
)

target_link_libraries(basic_example PUBLIC basic_example_shared skity::example_common)
