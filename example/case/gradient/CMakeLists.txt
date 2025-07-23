# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(gradient_example
    gradient_example.cc
    gradient_example.hpp
    main.cc
)

target_link_libraries(gradient_example PUBLIC skity::example_common)