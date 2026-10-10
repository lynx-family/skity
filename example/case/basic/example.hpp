// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef EXAMPLE_BASIC_EXAMPLE_HPP
#define EXAMPLE_BASIC_EXAMPLE_HPP

// Dual-API example: this header and example.cc compile against BOTH the
// legacy C++ API and the header-only RAII wrapper. The SKITY_EXAMPLE_HPP
// definition selects the backend; `sk::` is the common spelling so the
// drawing code below is written once. The frame glue in main.cc stays on
// the legacy API in both modes and lends the canvas through
// skity_bridge.hpp when the wrapper is selected.

#ifdef SKITY_EXAMPLE_HPP
#include <skity_hpp/skity.hpp>
namespace sk = skity::raii;
#else
#include <skity/skity.hpp>
namespace sk = skity;
#endif

namespace skity::example::basic {
void draw_canvas(sk::Canvas& canvas);
}

#endif  // EXAMPLE_BASIC_EXAMPLE_HPP
