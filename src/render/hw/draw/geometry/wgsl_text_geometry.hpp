// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TEXT_GEOMETRY_HPP
#define SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TEXT_GEOMETRY_HPP

#include "src/render/hw/draw/hw_wgsl_geometry.hpp"

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

class WGSLTextGeometry : public HWWGSLGeometry {
 public:
  static constexpr const char* kTextCommonVertex = R"(
    struct TextVSInput {
        @location(0) a_pos : vec2<f32>,
        @location(1) a_uv  : vec2<f32>,
    };

    fn get_texture_index(u: f32) -> i32 {
        return i32(u) >> 14;
    }

    fn get_texture_uv(uv: vec2<f32>) -> vec2<f32> {
        var u: i32 = i32(uv.x);

        return vec2<f32>(f32(u & 0x3FFF), uv.y);
    }

    @group(0) @binding(0) var<uniform> common_slot: CommonSlot;
  )";

  explicit WGSLTextGeometry(ArrayList<GlyphRect, 16> glyph_rects)
      : glyph_rects_(std::move(glyph_rects)) {}

  ~WGSLTextGeometry() override = default;

  const std::vector<GPUVertexBufferLayout>& GetBufferLayout() const override;

  const char* GetEntryPoint() const override { return "vs_main"; }

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  ArrayList<GlyphRect, 16> glyph_rects_;
};

class WGSLTextSolidColorGeometry : public WGSLTextGeometry {
 public:
  explicit WGSLTextSolidColorGeometry(ArrayList<GlyphRect, 16> glyph_rects)
      : WGSLTextGeometry(std::move(glyph_rects)) {}

  ~WGSLTextSolidColorGeometry() override = default;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override {
    return "TextSolidColorVertexWGSL";
  }
};

class WGSLTextGradientGeometry : public WGSLTextGeometry {
 public:
  explicit WGSLTextGradientGeometry(ArrayList<GlyphRect, 16> glyph_rects,
                                    const Matrix& local_matrix,
                                    const Matrix& local_to_device)
      : WGSLTextGeometry(std::move(glyph_rects)) {
    Matrix local_inv;
    local_matrix.Invert(&local_inv);
    Matrix device_to_local;
    local_to_device.Invert(&device_to_local);
    inv_matrix_ = local_inv * device_to_local;
  }

  ~WGSLTextGradientGeometry() override = default;

  std::string GenSourceWGSL() const override;

  std::string GetShaderName() const override {
    return "TextGradientVertexWGSL";
  }

  void PrepareCMD(Command* cmd, HWDrawContext* context, const Matrix& transform,
                  float clip_depth, Command* stencil_cmd) override;

 private:
  Matrix inv_matrix_;
};

}  // namespace skity

#endif  // SRC_RENDER_HW_DRAW_GEOMETRY_WGSL_TEXT_GEOMETRY_HPP
