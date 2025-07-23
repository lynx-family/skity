# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(recorder_example
    recorder_example.cc
    recorder_example.hpp
    main.cc
)

target_link_libraries(recorder_example PUBLIC basic_example_shared skity::example_common)
