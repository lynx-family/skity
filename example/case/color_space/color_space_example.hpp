// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef EXAMPLE_CASE_COLOR_SPACE_COLOR_SPACE_EXAMPLE_HPP
#define EXAMPLE_CASE_COLOR_SPACE_COLOR_SPACE_EXAMPLE_HPP

namespace skity {
class Canvas;
}

namespace skity::example::color_space {

// Draws the color space showcase panels (see color_space_example.cc for
// what each panel demonstrates).
void draw_color_space_example(skity::Canvas* canvas);

// Renders the same showcase into a Display P3 tagged software bitmap and
// saves it as a PNG carrying the gamut metadata (gAMA/cHRM chunks), so
// ColorSync aware viewers display it as wide gamut. Prints the output path.
void export_display_p3_png();

}  // namespace skity::example::color_space

#endif  // EXAMPLE_CASE_COLOR_SPACE_COLOR_SPACE_EXAMPLE_HPP
