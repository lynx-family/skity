// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TEXT_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TEXT_GEOMETRY_HPP

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/utils/arena_allocator.hpp"
#include "src/utils/array_list.hpp"
#include "src/utils/batch_group.hpp"

namespace skity {

struct GlyphRect {
  GlyphRect(Vec4 vertex_coord, Vec2 texture_coord_tl, Vec2 texture_coord_br)
      : vertex_coord(vertex_coord),
        texture_coord_tl(texture_coord_tl),
        texture_coord_br(texture_coord_br) {}

  Vec4 vertex_coord;
  Vec2 texture_coord_tl;
  Vec2 texture_coord_br;
};

struct GlyphRectBatch {
  ArrayList<GlyphRect, 16> glyph_rects;
  Rect bounds = Rect::MakeEmpty();
};

std::vector<GlyphRectBatch> BuildGlyphRectBatches(
    ArrayList<GlyphRect, 16> glyph_rects, const Matrix& transform,
    ArenaAllocator* arena_allocator, bool split_overlapping_glyphs);

enum class TextAtlasEffect {
  kA8Coverage,
  kSDFCoverage,
  kColor,
  kColorSwizzleRB,
};

class WGSLTextGeometry final : public HWWGSLGeometry {
 public:
  using BatchedTexture = std::array<std::shared_ptr<GPUTexture>, 4>;

  WGSLTextGeometry(std::vector<BatchGroup<GlyphRect>> glyph_rects,
                   BatchedTexture textures, std::shared_ptr<GPUSampler> sampler,
                   TextAtlasEffect effect,
                   std::optional<Matrix> device_to_local = std::nullopt);

  ~WGSLTextGeometry() override = default;

  static std::vector<GPUVertexBufferLayout> GetBufferLayout();

  HWFunctionBaseKey GetMainKey() const override;

  HWFunctionBaseKey GetFSSubKey() const override;

  void WriteVSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteVSUniforms(std::stringstream& ss) const override;

  void WriteVSInput(std::stringstream& ss) const override;

  void WriteVSMain(std::stringstream& ss) const override;

  std::optional<std::vector<std::string>> GetVarings() const override;

  void WriteFSFunctionsAndStructs(std::stringstream& ss) const override;

  void WriteFSUniforms(std::stringstream& ss) const override;

  void WriteFSColor(std::stringstream& ss) const override;

  void WriteFSAlphaMask(std::stringstream& ss) const override;

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  std::vector<BatchGroup<GlyphRect>> glyph_rects_;
  BatchedTexture textures_;
  std::shared_ptr<GPUSampler> sampler_;
  TextAtlasEffect effect_;
  std::optional<Matrix> device_to_local_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TEXT_GEOMETRY_HPP
