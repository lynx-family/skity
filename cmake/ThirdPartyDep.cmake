# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

# glm
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "enable GLM_BUILD_LIBRARY")
set(GLM_BUILD_INSTALL OFF CACHE BOOL "enable GLM_BUILD_INSTALL")
add_subdirectory(third_party/glm)
target_link_libraries(skity PRIVATE glm::glm-header-only)

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
  include(ExternalProject)

  set(SKITY_ZLIB_C_FLAGS "")

  # check if build on linux
  if (LINUX OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(SKITY_ZLIB_C_FLAGS "-fPIC")
  endif()

  ExternalProject_Add(zlib
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/third_party/zlib
    PREFIX ${CMAKE_BINARY_DIR}/third_party/zlib_build
    CMAKE_ARGS
    -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/third_party/zlib
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_C_FLAGS=${SKITY_ZLIB_C_FLAGS}
  )

  target_include_directories(skity PRIVATE ${CMAKE_BINARY_DIR}/third_party/zlib/include)
  target_link_directories(skity PRIVATE ${CMAKE_BINARY_DIR}/third_party/zlib/lib)
  if (WIN32)
    # FIXME: zlib in msvc output a different name
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
      target_link_libraries(skity PRIVATE zlibstaticd)
    else()
      target_link_libraries(skity PRIVATE zlibstatic)
    endif()
  else()
    target_link_libraries(skity PRIVATE ${CMAKE_BINARY_DIR}/third_party/zlib/lib/libz.a)
  endif()

  add_dependencies(skity zlib)

  set(USE_THIRD_PARTY_ZLIB ON)
endif()

# trace
if (SKITY_TRACE)
  message("Open Skity trace")
  target_compile_definitions(skity PRIVATE SKITY_ENABLE_TRACING=1)
endif()
