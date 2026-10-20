# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(
    skp-loader
    main.cc
)

# skity::capi is the public C API the example drives (headers + library).
target_link_libraries(skp-loader PUBLIC skity::example_common skity::io skity::capi)
