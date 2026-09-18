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

# Dual-API variant: the same sources compiled with SKITY_EXAMPLE_HPP, so the
# drawing code runs through the header-only RAII wrapper over the C ABI
# while the frame glue (window / context) stays on the legacy C++ API and
# lends the canvas through skity_bridge.hpp. Compilation failures here are
# the wrapper coverage checklist; runtime parity is checked by comparing
# both binaries' output.
if (${SKITY_CAPI_MODULE})
  add_executable(basic_example_hpp
      main.cc
      example.cc
  )

  target_compile_definitions(basic_example_hpp PRIVATE SKITY_EXAMPLE_HPP=1)
  target_include_directories(basic_example_hpp PRIVATE ${CMAKE_SOURCE_DIR}/example)
  target_link_libraries(basic_example_hpp PRIVATE skity::capi skity::example_common)
endif()
