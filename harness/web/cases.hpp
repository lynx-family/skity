// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef HARNESS_WEB_CASES_HPP
#define HARNESS_WEB_CASES_HPP

#include <cstdint>
#include <skity/graphic/color_space.hpp>
#include <skity/render/canvas.hpp>

namespace skity {
namespace web_harness {

/**
 * A geometry-only smoke test case. The render callback draws with the public
 * Canvas API only (no text, no image files, no codec) so the case set can run
 * inside a wasm build with SKITY_CODEC_MODULE=OFF.
 *
 * Draw logic is extracted from the corresponding test/golden cases; keep the
 * golden case names to make tracing back easy.
 */
struct GeometryCase {
  const char* name;
  uint32_t width;
  uint32_t height;
  void (*render)(Canvas* canvas);
  /**
   * Color space the case's surface is created in: the render target's
   * encoding and the working space of every draw. The default (sRGB) keeps
   * the pre-color-space behavior.
   */
  ColorSpace target_space;
};

/** Table of the smoke test cases, terminated implicitly by GetCaseCount(). */
const GeometryCase* GetGeometryCases();

uint32_t GetCaseCount();

}  // namespace web_harness
}  // namespace skity

#endif  // HARNESS_WEB_CASES_HPP
