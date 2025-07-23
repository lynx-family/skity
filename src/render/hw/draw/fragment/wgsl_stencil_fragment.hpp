// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_STENCIL_FRAGMENT_HPP
#define SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_STENCIL_FRAGMENT_HPP

#include "src/render/hw/draw/hw_wgsl_fragment.hpp"

namespace skity {

class WGSLStencilFragment : public HWWGSLFragment {
 public:
  ~WGSLStencilFragment() override = default;

  std::string GetShaderName() const override;

  std::string GenSourceWGSL() const override;

  const char* GetEntryPoint() const override;

  uint32_t NextBindingIndex() const override { return 0; }

  void PrepareCMD(Command* cmd, HWDrawContext* context) override {}
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_FRAGMENT_WGSL_STENCIL_FRAGMENT_HPP
