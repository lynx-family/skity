# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

cmake_minimum_required(VERSION 3.15.0)

project(Skity)

# Compatible for MSVC runtime library
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

if(POLICY CMP0079)
  cmake_policy(SET CMP0079 NEW)
endif()

set(SKITY_VERSION 0.1.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_definitions(-DSKITY_RELEASE)
endif()

set(SKITY_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_CXX_EXTENSIONS OFF) # disable -std=gnu++11 by default


# options should set before define skity library
include(cmake/Options.cmake)

if (MSVC AND SKITY_TEST)
  # MSVC needs to set export all symbols for test since UT case needs to access private symbols
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON CACHE BOOL "Force MSVC export all symbols")
endif()

# define library
if(EMSCRIPTEN)
  add_library(skity STATIC)
else()
  add_library(skity SHARED)

  # binary size optimization
  if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    if (ANDROID)
      target_link_options(skity PUBLIC "-flto")
      target_link_options(skity PUBLIC "-Wl,--exclude-libs,ALL" "-Wl,--gc-sections" "-fuse-ld=lld")

      target_compile_options(skity PUBLIC
        "-flto"
        "-Oz"
        "-fomit-frame-pointer"
        "-fno-sanitize=safe-stack"
        "-fno-strict-aliasing"
      )
    endif()
  endif()

  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    # generate link map file
    if (ANDROID)
      target_link_options(skity PUBLIC
        "-Wl,-Map,${CMAKE_BINARY_DIR}/libskity.so.map"
        # make sure the link map file format can be parsed by supersize
      )
    elseif(${CMAKE_SYSTEM_NAME} STREQUAL "iOS")
      target_link_options(skity PUBLIC
        "LINKER:-map,${CMAKE_BINARY_DIR}/libskity.so.map"
      )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
      # using Clang
      target_link_options(skity PUBLIC
        "LINKER:-Map,${CMAKE_BINARY_DIR}/libskity.so.map"
      )
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      # using GCC
      target_link_options(skity PUBLIC
        "LINKER:-Map,${CMAKE_BINARY_DIR}/libskity.so.map"
      )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
      target_link_options(skity PUBLIC
        "LINKER:-map,${CMAKE_BINARY_DIR}/libskity.so.map"
      )
    endif()
  endif()
endif()

# public header include
target_include_directories(skity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(skity PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
set_property(TARGET skity PROPERTY CXX_STANDARD 17)

if (NOT SKITY_NO_PREBUILD_SHADER)
  # shaders gen header
  target_include_directories(skity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gen)
  target_include_directories(skity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/shaders/bindings)
endif()

target_include_directories(
  skity
  PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

target_compile_features(skity PUBLIC cxx_std_17)

add_library(skity::skity ALIAS skity)

# default config
include(GNUInstallDirs)

# platform
include(cmake/Platform.cmake)

# third party deps
include(cmake/ThirdPartyDep.cmake)

# add wgx module into build process
add_subdirectory(module/wgx)

# macro for wgx
target_compile_definitions(skity PRIVATE -DSKITY_WGX=1)

if(${SKITY_LOG})
  # no exception
  target_compile_definitions(skity PRIVATE -DSPDLOG_NO_EXCEPTIONS)
  target_compile_definitions(skity PRIVATE -DSKITY_LOG)
  # FIXME: disable -Wdeprecated-declarations warnings
  if (MSVC)
    target_compile_options(skity PRIVATE /wd4996)
  else()
    target_compile_options(skity PRIVATE -Wno-deprecated-declarations)
  endif()

  if(ANDROID)
    target_link_libraries(skity PRIVATE log)
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    find_library(hilog-lib hilog_ndk.z)
    target_link_libraries(skity PRIVATE ${hilog-lib})
  endif()
endif()

if(${SKITY_VK_BACKEND})
  target_compile_definitions(skity PUBLIC -DSKITY_VULKAN)
endif()

if(${SKITY_GL_BACKEND})
  target_compile_definitions(skity PUBLIC -DSKITY_OPENGL)
endif()

if(${SKITY_MTL_BACKEND})
  target_compile_definitions(skity PRIVATE -DSKITY_METAL)
endif()

if(${SKITY_SW_RENDERER})
  target_compile_definitions(skity PUBLIC -DSKITY_CPU)
endif()

target_compile_definitions(skity PRIVATE -DDISABLE_SKITY_EXPERIMENTAL_WARNINGS)

if(${SKITY_CODEC_MODULE})
  add_subdirectory(module/codec)
endif()

# header files
add_subdirectory(include)

# source files
add_subdirectory(src)

# source group for Xcode and Visual Studio
get_target_property(SKITY_SRC skity SOURCES)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/.. FILES ${SKITY_SRC})

if(${SKITY_EXAMPLE})
  add_subdirectory(example)
endif()

if(${SKITY_TEST})
  add_subdirectory(test)
endif()

# package config file
include(CMakePackageConfigHelpers)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/Config.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/skityConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skity
)

# version
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/skityConfigVersion.cmake
  VERSION ${SKITY_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/skityConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/skityConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skity
)

# install library
install(
  TARGETS skity EXPORT skity-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# install config file
install(
  EXPORT skity-targets
  FILE skityTargets.cmake
  NAMESPACE skity::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/skity
)
