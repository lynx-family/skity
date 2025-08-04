# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(fake_3d_example
    fake_3d_example.cc
    fake_3d_example.hpp
    main.cc
)

target_link_libraries(fake_3d_example PUBLIC skity::example_common)
target_link_libraries(fake_3d_example PRIVATE glm::glm-header-only)
