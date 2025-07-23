# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(image_example
    image_example.cc
    image_example.hpp
    main.cc
)

target_compile_definitions(image_example PRIVATE -DEXAMPLE_IMAGE_ROOT="${CMAKE_SOURCE_DIR}/example/images/")

if (${SKITY_GL_BACKEND})
  target_sources(
    image_example
    PRIVATE
    image_gl.cc
  )
endif()

if (${SKITY_MTL_BACKEND})
  target_sources(
    image_example
    PRIVATE
    image_mtl.mm
  )
endif()

target_link_libraries(image_example PUBLIC skity::example_common)