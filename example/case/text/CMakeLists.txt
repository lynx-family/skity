# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(text_example
    text_example.hpp
    main.cc
)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_sources(text_example PRIVATE win/text_example.cc)
else()
    target_sources(text_example PRIVATE text_example.cc)
endif()

target_link_libraries(text_example PUBLIC skity::example_common)
target_compile_definitions(text_example PRIVATE -DEXAMPLE_IMAGE_ROOT="${CMAKE_SOURCE_DIR}/example/images/")
