// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP
#define SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP

#include <optional>

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_draw.hpp"

namespace skity {

class HWDynamicTextDraw final : public HWDynamicDraw {
 public:
  HWDynamicTextDraw(Matrix draw_transform, const Matrix& glyph_transform,
                    ArrayList<GlyphRect, 16> glyph_rects, Paint paint,
                    WGSLTextGeometry::BatchedTexture textures,
                    std::shared_ptr<GPUSampler> sampler, TextAtlasEffect effect,
                    bool is_stroke,
                    std::optional<Matrix> device_to_local = std::nullopt);

  ~HWDynamicTextDraw() override = default;

  HWDrawType GetDrawType() const override { return HWDrawType::kText; }

  bool OnMergeIfPossible(HWDraw* draw) override;

  static Matrix CalcTransform(const Matrix& canvas_transform,
                              const Matrix& text_transform);

  static Matrix CalcSDFTransform(const Matrix& transform, float scale);

 protected:
  void OnGenerateDrawStep(ArrayList<HWDrawStep*, 2>& steps,
                          HWDrawContext* context) override;

 private:
  std::vector<BatchGroup<GlyphRect>> glyph_rects_;
  WGSLTextGeometry::BatchedTexture textures_;
  std::shared_ptr<GPUSampler> sampler_;
  TextAtlasEffect effect_;
  bool is_stroke_;
  std::optional<Matrix> device_to_local_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_HW_DYNAMIC_TEXT_DRAW_HPP
