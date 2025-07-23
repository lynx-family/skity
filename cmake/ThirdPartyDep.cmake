# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# glm
set(GLM_BUILD_INSTALL ON CACHE BOOL "enable GLM_BUILD_INSTALL" FORCE)
add_subdirectory(third_party/glm)
target_link_libraries(skity PUBLIC glm::glm)

# pugixml
set(PUGIXML_NO_EXCEPTIONS ON CACHE BOOL "enable PUGIXML_NO_EXCEPTIONS")
add_subdirectory(third_party/pugixml)
target_link_libraries(skity PUBLIC pugixml::pugixml)

# spdlog
if(${SKITY_LOG})
  add_subdirectory(third_party/fmt)
  target_link_libraries(skity PRIVATE fmt::fmt-header-only)
endif()

# Vulkan deps
if(${SKITY_VK_BACKEND})
  target_include_directories(skity PRIVATE third_party/Vulkan-Headers/include)
endif()

# OpenGL header file
if(${SKITY_GL_BACKEND})
  target_include_directories(skity PRIVATE third_party/OpenGL)
endif()

# Vulkan memory allocator
if(${SKITY_VK_BACKEND})
  # set vulkan headers
  if (NOT ANDROID)
    # android can use system vulkan headers from NDK
    set(VULKAN_HEADERS_INSTALL_DIR "third_party/Vulkan-Headers" CACHE PATH "Vulkan-Headers")
  endif()

  target_compile_definitions(skity PRIVATE VK_NO_PROTOTYPES=1)
  add_subdirectory(third_party/volk)
  target_link_libraries(skity PRIVATE volk::volk)
endif()

# json parser
if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  message("SKITY_HARMONY")
  set(JSONCPP_WITH_TESTS OFF)
  add_subdirectory(third_party/jsoncpp)

  if(TARGET jsoncpp_static)
    add_library(JsonCpp::JsonCpp ALIAS jsoncpp_static)
  elseif(TARGET jsoncpp_lib)
    add_library(JsonCpp::JsonCpp ALIAS jsoncpp_lib)
  endif()

  target_link_libraries(skity PUBLIC JsonCpp::JsonCpp)
endif()

# zlib
# not iOS or not macOS and not Android and not OHOS
if (NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND NOT ANDROID AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS" AND NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS" AND NOT SKITY_USE_SELF_LIBCXX)
  # FIXME: some linker under linux may fail to link zlib
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--undefined-version")
  include_directories(third_party/zlib)
  include_directories(${CMAKE_BINARY_DIR}/third_party/zlib)
  add_subdirectory(third_party/zlib)
  target_link_libraries(skity PRIVATE zlibstatic)

  set(USE_THIRD_PARTY_ZLIB ON)
endif()

# trace
if (SKITY_TRACE)
  message("Open Skity trace")
  target_compile_definitions(skity PRIVATE SKITY_ENABLE_TRACING=1)
endif()
