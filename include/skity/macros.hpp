// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_MACROS_HPP
#define INCLUDE_SKITY_MACROS_HPP

#if defined(_WIN32) || defined(_WIN64)
#define SKITY_WIN
#elif defined(__APPLE__) || defined(__MACH__)
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR == 1 || TARGET_OS_IPHONE == 1
#ifndef SKITY_IOS
#define SKITY_IOS 1
#endif
#elif TARGET_OS_MAC == 1
#ifndef SKITY_MACOS
#define SKITY_MACOS 1
#endif
#endif
#elif defined(__ANDROID__)
#define SKITY_ANDROID
#elif defined(__OHOS_FAMILY__) && !defined(SKITY_HARMONY)
#define SKITY_HARMONY
#elif defined(__linux__)
#define SKITY_LINUX
#elif defined(__EMSCRIPTEN__) || defined(__wasm__) || defined(__wasm32__) || \
    defined(__wasm64__)
#ifndef SKITY_WASM
#define SKITY_WASM 1
#endif
#endif

// wasm do not support shared library
// iOS use cocoapods also not use shared library
// all other platform has been changed to use shared library
#if !defined(SKITY_WASM) && !defined(SKITY_IOS)
#ifndef SKITY_DLL
#define SKITY_DLL
#endif
#endif

#ifdef SKITY_DLL

#if defined(SKITY_WIN) || defined(_MSC_VER)

// Building a shared library requires dllexport, using one requires dllimport.
// Both directions have to be distinguished, otherwise a consumer resolving a
// public data symbol cannot link (MSVC matches the import library only against
// the __imp_ form, for example "?g_interface@@3PEAUGLInterface@@EA" versus
// "__imp_?g_interface@@3PEAUGLInterface@@EA"), and function calls lose the
// import thunk.
//
//   SKITY_EXPORTS - compiling the shared library itself.
//   SKITY_IMPORTS - linked against the shared library, the default here. The
//                   skity CMake target publishes this as an INTERFACE
//                   definition so every consumer gets it automatically.
//   SKITY_STATIC  - linked against a static library.
#if defined(SKITY_EXPORTS)
#define SKITY_API __declspec(dllexport)
#elif defined(SKITY_STATIC)
#define SKITY_API
#else
#define SKITY_API __declspec(dllimport)
#endif

#else
#define SKITY_API __attribute__((visibility("default")))
#endif

#else
#define SKITY_API
#endif

#if defined(DISABLE_SKITY_EXPERIMENTAL_WARNINGS)
#define SKITY_EXPERIMENTAL
#define SKITY_EXPERIMENTAL_API SKITY_API
#else

#if defined(__GNUC__)

#define SKITY_EXPERIMENTAL \
  __attribute__((          \
      deprecated("Skity Experimental API - may change or be removed")))
#define SKITY_EXPERIMENTAL_API SKITY_API SKITY_EXPERIMENTAL

#else

#define SKITY_EXPERIMENTAL \
  [[deprecated("Skity Experimental API - may change or be removed")]]
#define SKITY_EXPERIMENTAL_API SKITY_API SKITY_EXPERIMENTAL

#endif

#endif

#if defined(__ARM_NEON__) || defined(__arm__) || defined(__aarch64__)
#define SKITY_ARM_NEON
#endif

#endif  // INCLUDE_SKITY_MACROS_HPP
