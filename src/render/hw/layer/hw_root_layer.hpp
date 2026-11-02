// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_LAYER_HW_ROOT_LAYER_HPP
#define SRC_RENDER_HW_LAYER_HW_ROOT_LAYER_HPP

#include "src/render/hw/hw_layer.hpp"

namespace skity {

class HWRootLayer : public HWLayer {
 public:
  HWRootLayer(uint32_t width, uint32_t height, const Rect& bounds,
              GPUTextureFormat format);

  ~HWRootLayer() override = default;

  void SetClearSurface(bool clear) { clear_surface_ = clear; }

  bool NeedClearSurface() const { return clear_surface_; }

  GPUColor GetClearColor() const { return clear_color_; }

  void EnableInitialClearPromotion() { allow_clear_promotion_ = true; }

  bool CanPromoteInitialClear() const {
    return allow_clear_promotion_ && IsInitialPassEmpty();
  }

  void SetInitialClearColor(GPUColor color) {
    clear_surface_ = true;
    clear_color_ = color;
  }

  virtual bool IsValid() const { return true; }

 private:
  bool clear_surface_ = true;
  bool allow_clear_promotion_ = false;
  GPUColor clear_color_ = {};
};

}  // namespace skity

#endif  // SRC_RENDER_HW_LAYER_HW_ROOT_LAYER_HPP
