// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/basic/example.hpp"
#include "common/app.hpp"

#ifdef SKITY_EXAMPLE_HPP
#include <skity_hpp/skity_bridge.hpp>
#endif

class BasicExampleCase : public skity::example::WindowClient {
 public:
  BasicExampleCase() = default;

  ~BasicExampleCase() override = default;

  void OnDraw(skity::GPUContext*, skity::Canvas* canvas) override {
    canvas->DrawColor(skity::Color_WHITE);

#ifdef SKITY_EXAMPLE_HPP
    // Wrapper mode: the frame glue stays on the legacy API and lends the
    // canvas through skity_bridge.hpp; the drawing code above then drives
    // the same canvas through the C ABI and the RAII layer. This is the
    // mid-migration mixed usage animax is expected to settle on.
    skity::raii::Canvas view(skity_canvas_from_native(canvas));
    skity::example::basic::draw_canvas(view);
    // Non-owning handle: destroying it only reclaims the small wrapper
    // struct, the surface keeps the canvas alive.
    skity_canvas_destroy(view.get());
#else
    skity::example::basic::draw_canvas(*canvas);
#endif
  }
};

int main(int argc, const char** argv) {
  BasicExampleCase basic_case{};

  return skity::example::StartExampleApp(argc, argv, basic_case, 1000, 800,
                                         "Basic Example");
}
