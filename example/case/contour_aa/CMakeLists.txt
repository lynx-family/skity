# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(contour_aa_example
    contour_aa_example.cc
    contour_aa_example.hpp
    main.cc
)

target_link_libraries(contour_aa_example PUBLIC skity::example_common)
