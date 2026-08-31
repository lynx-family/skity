# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

add_executable(skity-c-api-vk main.cc)

if (NOT TARGET volk::volk)
  set(VULKAN_HEADERS_INSTALL_DIR
      ${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers
      CACHE PATH "Use bundled Vulkan headers for examples" FORCE)
  set(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "Disable Volk Vulkan lookup" FORCE)
  add_subdirectory(${CMAKE_SOURCE_DIR}/third_party/volk third_party/volk)
  target_include_directories(volk PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include)
endif()

if (APPLE)
  target_sources(skity-c-api-vk PRIVATE native_window_macos.mm)
  target_compile_options(skity-c-api-vk PRIVATE -fobjc-arc)
  target_link_libraries(skity-c-api-vk PRIVATE "-framework QuartzCore")
endif()

target_include_directories(skity-c-api-vk PRIVATE
  ${CMAKE_SOURCE_DIR}/third_party/Vulkan-Headers/include
)

target_link_libraries(skity-c-api-vk PRIVATE skity::capi glfw volk::volk)
set_target_properties(skity-c-api-vk PROPERTIES CXX_STANDARD 17)
