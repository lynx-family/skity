// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/fragment/wgsl_stencil_fragment.hpp"

namespace skity {

std::string WGSLStencilFragment::GetShaderName() const {
  return "StencilFragmentWGSL";
}

std::string WGSLStencilFragment::GenSourceWGSL() const {
  return R"(
        @fragment
        fn fs_main() -> @location(0) vec4<f32> {
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        }
    )";
}

const char* WGSLStencilFragment::GetEntryPoint() const { return "fs_main"; }

}  // namespace skity
