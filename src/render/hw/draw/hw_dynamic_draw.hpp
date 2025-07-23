// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_DRAW_HPP

#include <memory>
#include <vector>

#include "src/render/hw/draw/hw_draw_step.hpp"
#include "src/render/hw/hw_draw.hpp"

namespace skity {

class HWDynamicDraw : public HWDraw {
 public:
  HWDynamicDraw(Matrix transform, BlendMode blend_mode)
      : HWDraw(transform), blend_mode_(blend_mode), steps_(), commands_() {
    steps_.reserve(2);
  }

  ~HWDynamicDraw() override = default;

  void Draw(GPURenderPass* render_pass) override {
    for (const auto& cmd : commands_) {
      render_pass->AddCommand(cmd);
    }
  }

 protected:
  HWDrawState OnPrepare(HWDrawContext* context) override;

  void OnGenerateCommand(HWDrawContext* context, HWDrawState state) override;

  virtual void OnGenerateDrawStep(std::vector<HWDrawStep*>& steps,
                                  HWDrawContext* context) = 0;

 private:
  BlendMode blend_mode_;
  std::vector<HWDrawStep*> steps_;
  ArrayList<Command*, 32> commands_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_DRAW_HPP
