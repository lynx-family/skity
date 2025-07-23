# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(lock_canvas_example
    main.cc
)

if (${SKITY_GL_BACKEND})
  target_compile_definitions(lock_canvas_example PRIVATE -DSKITY_EXAMPLE_GL_BACKEND=1)

  target_sources(lock_canvas_example PRIVATE surface_gl.cc)
endif()

if (${SKITY_MTL_BACKEND})
  target_compile_definitions(lock_canvas_example PRIVATE -DSKITY_EXAMPLE_MTL_BACKEND=1)
  target_sources(lock_canvas_example PRIVATE surface_mtl.mm)
  target_link_libraries(lock_canvas_example PRIVATE "-framework Metal")
endif()

target_link_libraries(lock_canvas_example PUBLIC skity::example_common)
