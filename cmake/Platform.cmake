# Copyright 2021 The Lynx Authors. All rights reserved.
# Licensed under the Apache License Version 2.0 that can be found in the
# LICENSE file in the root directory of this source tree.

if(EMSCRIPTEN)
  # Build Skity with emsdk
  add_definitions(-DSKITY_WASM)
elseif(WIN32)
  add_definitions(-DSKITY_WIN)

  # Fixme to solve NIM MAX macro defined in windows.h
  add_definitions(-DNOMINMAX)
  set(BUILD_SHARED_LIBS TRUE)

  # specify dll outpu directory
  set(SKITY_DLL_DIR ${CMAKE_CURRENT_BINARY_DIR})
  target_compile_options(
    skity
    PUBLIC
    PRIVATE
    /EHs-c- # disable exceptions
  )

else()
  target_compile_options(
    skity
    PUBLIC
    PRIVATE -fno-exceptions)

    if (NOT ${SKITY_TEST})
      target_compile_options(
        skity
        PUBLIC
        PRIVATE
        -fvisibility=hidden
        -fvisibility-inlines-hidden
        -fno-rtti
      )
    endif()

    if (APPLE)
      # FIXME : add libstdc++ in link library
      target_link_libraries(skity PRIVATE -lc++)
    endif()
endif()

# dependencies
if(EMSCRIPTEN)
  # use emsd port freetype
  add_definitions(-DENABLE_TEXT_RENDER=1)

  # use emscripten port freetype
  set(FREETYPE_FOUND True)

  target_compile_options(skity PUBLIC "-s" "USE_FREETYPE=1")
  set_target_properties(skity PROPERTIES LINK_FLAGS "-s USE_FREETYPE=1")
elseif(ANDROID)
  # TODO support android build
  message("build for Android")
  # xml parser
  target_link_libraries(skity PUBLIC pugixml::pugixml)
  if (${SKITY_USE_ASAN})
    message("build with asan")
    target_compile_options(skity PRIVATE -fsanitize=address -fno-omit-frame-pointer)
    set_target_properties(skity PROPERTIES LINK_FLAGS -fsanitize=address)
  else()
    message("build without asan")
    target_compile_options(skity PRIVATE -fomit-frame-pointer -fno-sanitize=safe-stack -Werror -Wunused-variable -Wunused-function)
  endif()

  # arm64
  if (CMAKE_ANDROID_ARCH_ABI STREQUAL "arm64-v8a" OR CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
    target_link_options(skity PUBLIC "-Wl,-z,max-page-size=16384" "-Wl,-z,common-page-size=16384")
  endif()
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Darwin")
  message("build for macos")
  target_compile_options(skity PRIVATE "-fobjc-arc")
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Ios")
  message("build for ios")
  target_compile_options(skity PRIVATE "-fobjc-arc")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  message("build for Harmony")
  add_definitions(-DSKITY_HARMONY=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message("build for Linux")

  if (${SKITY_USE_SELF_LIBCXX})
    # FIXME: Lynx use custom sysroot and libcxx when build for unittest on Linux
    include_directories(${CMAKE_SOURCE_DIR}/third_party/libcxx/include)
    include_directories(${CMAKE_SOURCE_DIR}/third_party/libcxxabi/include)
    include_directories(${CMAKE_SOURCE_DIR}/build/secondary/third_party/libcxx/config)

    add_definitions(-D_LIBCPP_DISABLE_AVAILABILITY=1 -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -D_LIBCPP_ENABLE_THREAD_SAFETY_ANNOTATIONS)
    set(CMAKE_CXX_FLAGS "-nostdinc++")

    include(cmake/Libcxx.cmake)

    target_link_libraries(skity PRIVATE $<TARGET_OBJECTS:third_party__libcxx> $<TARGET_OBJECTS:third_party__libcxxabi>)
  endif()
endif()
