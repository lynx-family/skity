// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_LAYER_HW_ROOT_LAYER_HPP
#define SRC_RENDER_HW_LAYER_HW_ROOT_LAYER_HPP

#include "src/render/hw/hw_layer.hpp"
#include "src/utils/arena_allocator.hpp"

namespace skity {

class HWRootLayer : public HWLayer {
 public:
  HWRootLayer(uint32_t width, uint32_t height, const Rect& bounds,
              GPUTextureFormat format);

  ~HWRootLayer() override = default;

  void SetClearSurface(bool clear) { clear_surface_ = clear; }

  bool NeedClearSurface() const { return clear_surface_; }

  virtual bool IsValid() const { return true; }

 private:
  bool clear_surface_ = true;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_LAYER_HW_ROOT_LAYER_HPP
