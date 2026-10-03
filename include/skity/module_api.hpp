// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_MODULE_API_HPP
#define INCLUDE_SKITY_MODULE_API_HPP

// skity ships more than one shared library: the core "skity" library and the
// optional "skity-codec" module, each with a public surface of its own. Every
// surface needs its own export/import switch, because an executable can link
// both at once: the symbols skity-codec publishes have to be dllexport while
// that library is built and dllimport in its consumer, which a single
// SKITY_API cannot express for two libraries at the same time.
//
// SKITY_API itself lives in macros.hpp and covers the core "skity" library.
// The matching CMake side is:
//   core   : skity          PRIVATE SKITY_EXPORTS, INTERFACE SKITY_IMPORTS
//   module : skity-codec    PRIVATE SKITY_CODEC_EXPORTS
//   static : any consumer   PRIVATE SKITY_STATIC
//
// skity-io is a static library, so its own public classes carry no annotation
// at all and only the skity:: symbols they touch use SKITY_API.
//
// See the comment on SKITY_API in macros.hpp for why the directions have to be
// distinguished on Windows at all.

#if defined(_WIN32) || defined(_WIN64) || defined(_MSC_VER)

#if defined(SKITY_STATIC)
#define SKITY_CODEC_API
#elif defined(SKITY_CODEC_EXPORTS)
#define SKITY_CODEC_API __declspec(dllexport)
#else
#define SKITY_CODEC_API __declspec(dllimport)
#endif

#else

#define SKITY_CODEC_API __attribute__((visibility("default")))

#endif

#endif  // INCLUDE_SKITY_MODULE_API_HPP
