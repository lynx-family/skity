# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(save_layer_example
    save_layer_example.cc
    save_layer_example.hpp
    main.cc
)

target_link_libraries(save_layer_example PUBLIC basic_example_shared skity::example_common)
