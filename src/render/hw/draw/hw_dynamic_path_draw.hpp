// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_PATH_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_PATH_DRAW_HPP

#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <utility>

#include "src/render/hw/draw/hw_dynamic_draw.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

namespace skity {

class HWDynamicPathDraw : public HWDynamicDraw {
 public:
  HWDynamicPathDraw(Matrix transform, Path path, Paint paint, bool is_stroke,
                    bool use_gpu_tessellation);

  ~HWDynamicPathDraw() override = default;

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep *, 2> &steps,
                          HWDrawContext *context) override;

 private:
  HWWGSLGeometry *GenGeometry(HWDrawContext *context, bool aa) const;

  HWWGSLFragment *GenFragment(HWDrawContext *context) const;

 private:
  Path path_;
  Paint paint_;
  bool is_stroke_;
  bool use_gpu_tessellation_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_PATH_DRAW_HPP
