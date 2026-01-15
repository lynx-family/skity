// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/hw_pipeline_key.hpp"

#include <gtest/gtest.h>

#include <vector>

#include "skity/effect/color_filter.hpp"
#include "skity/effect/shader.hpp"
#include "skity/geometry/matrix.hpp"
#include "skity/geometry/rrect.hpp"
#include "skity/graphic/color.hpp"
#include "skity/graphic/path.hpp"
#include "src/effect/gradient_shader.hpp"
#include "src/render/hw/draw/fragment/wgsl_blur_filter.hpp"
#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_filter_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_draw_step.hpp"
#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"
#include "src/render/hw/draw/hw_dynamic_rrect_draw.hpp"
#include "src/render/hw/draw/hw_wgsl_fragment.hpp"
#include "src/render/hw/draw/hw_wgsl_geometry.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/utils/arena_allocator.hpp"

using namespace skity;

TEST(HWPipelineKey, ConvexPath_SolidColor) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = false;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);

  dynamic_path_draw.Prepare(&draw_context);

  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[0]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, ConcavePath_SolidColor) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.LineTo(300, 300);
  path.LineTo(10, 300);
  path.Close();
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = false;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 2);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, StrokePath_SolidColor) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = true;
  bool use_gpu_tessellation = false;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 2);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, ConvexPath_SolidColor_AA) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();
  Paint paint;
  paint.SetAntiAlias(true);
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = false;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 3);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPathAA));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid,
                                HWGeometryKeyType::kPathAA));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_PathAA");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor_AA");

  EXPECT_EQ(steps[2]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[2]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[2]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[2]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, ConcavePath_SolidColor_AA) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.LineTo(300, 300);
  path.LineTo(10, 300);
  path.Close();
  Paint paint;
  paint.SetAntiAlias(true);
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = false;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 3);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPathAA));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid,
                                HWGeometryKeyType::kPathAA));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_PathAA");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor_AA");

  EXPECT_EQ(steps[2]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[2]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[2]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[2]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, StrokePath_SolidColor_AA) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();
  Paint paint;
  paint.SetAntiAlias(true);
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = true;
  bool use_gpu_tessellation = false;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 3);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPathAA));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid,
                                HWGeometryKeyType::kPathAA));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_PathAA");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor_AA");

  EXPECT_EQ(steps[2]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kPath));
  EXPECT_EQ(steps[2]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[2]->GetVertexName(), "VS_Path");
  EXPECT_EQ(steps[2]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, ConvexPath_SolidColor_GPUTess) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessFill));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_TessPathFill");
  EXPECT_EQ(steps[0]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, ConcavePath_SolidColor_GPUTess) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.LineTo(300, 300);
  path.LineTo(10, 300);
  path.Close();
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 2);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessFill));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_TessPathFill");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessFill));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_TessPathFill");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor");
}

TEST(HWPipelineKey, StrokePath_SolidColor_GPUTess) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = true;
  bool use_gpu_tessellation = true;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();

  EXPECT_EQ(steps.size(), 2);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessStroke));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kStencil));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_TessPathStroke");
  EXPECT_EQ(steps[0]->GetFragmentName(), "StencilFragmentWGSL");

  EXPECT_EQ(steps[1]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessStroke));
  EXPECT_EQ(steps[1]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolid));
  EXPECT_EQ(steps[1]->GetVertexName(), "VS_TessPathStroke");
  EXPECT_EQ(steps[1]->GetFragmentName(), "FS_SolidColor");
}

// Gradient
TEST(HWPipelineKey, GradientLinear2OffsetFastColorFast) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();

  Point pts[2]{
      {10.f, 10.f, 0.f, 1.f},
      {100.f, 100.f, 0.f, 1.f},
  };
  Vec4 colors[2]{
      {0, 0, 0, 1},
      {1, 1, 1, 1},
  };
  auto linear_gradient = Shader::MakeLinear(pts, colors, nullptr, 2);
  Paint paint;
  paint.SetShader(linear_gradient);

  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessFill,
                                HWFragmentKeyType::kGradient));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(
                MakeMainKey(HWFragmentKeyType::kGradient, 0b11001001)));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_TessPathFill_Gradient");
  EXPECT_EQ(steps[0]->GetFragmentName(),
            "FS_GradientLinear2OffsetFastColorFast");
}

TEST(HWPipelineKey, GradientLinear4OffsetFastColorFast) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();

  Point pts[2]{
      {10.f, 10.f, 0.f, 1.f},
      {100.f, 100.f, 0.f, 1.f},
  };
  Vec4 colors[3]{
      {0, 0, 0, 1},
      {1, 1, 1, 1},
      {1, 1, 1, 1},

  };
  auto linear_gradient = Shader::MakeLinear(pts, colors, nullptr, 3);
  Paint paint;
  paint.SetShader(linear_gradient);

  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessFill,
                                HWFragmentKeyType::kGradient));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(
                MakeMainKey(HWFragmentKeyType::kGradient, 0b01010001)));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_TessPathFill_Gradient");
  EXPECT_EQ(steps[0]->GetFragmentName(), "FS_GradientLinear4OffsetFast");
}

TEST(HWPipelineKey, GradientRadial16) {
  Path path;
  path.MoveTo(10, 10);
  path.LineTo(100, 100);
  path.LineTo(200, 10);
  path.Close();

  Vec4 colors[11]{
      {0, 0, 0, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {0, 0, 0, 1},
      {1, 1, 1, 1}, {1, 1, 1, 1}, {0, 0, 0, 1}, {1, 1, 1, 1},
      {1, 1, 1, 1}, {0, 0, 0, 1}, {1, 1, 1, 1},

  };
  float pos[11]{0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0};
  auto radial_gradient =
      Shader::MakeRadial(Vec4{100, 100, 0, 1}, 50, colors, pos, 11);
  Paint paint;
  paint.SetShader(radial_gradient);

  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicPathDraw dynamic_path_draw(Matrix{}, path, paint, is_stroke,
                                             use_gpu_tessellation);
  dynamic_path_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_path_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kTessFill,
                                HWFragmentKeyType::kGradient));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(
                MakeMainKey(HWFragmentKeyType::kGradient, 0b00100010)));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_TessPathFill_Gradient");
  EXPECT_EQ(steps[0]->GetFragmentName(), "FS_GradientRadial16");
}

TEST(HWPipelineKey, RRect_SolidVertexColor) {
  RRect rrect;
  Paint paint;
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicRRectDraw dynamic_rrect_draw(Matrix{}, rrect, paint);
  dynamic_rrect_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_rrect_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kRRect,
                                HWFragmentKeyType::kSolidVertex));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolidVertex,
                                HWGeometryKeyType::kRRect));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_RRect_SolidVertexColor");
  EXPECT_EQ(steps[0]->GetFragmentName(), "FS_SolidVertexColor_RRect");
}

TEST(HWPipelineKey, RRect_SolidVertex_LinearToSRGBGammaFilter) {
  RRect rrect;
  Paint paint;
  paint.SetColorFilter(ColorFilters::LinearToSRGBGamma());
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicRRectDraw dynamic_rrect_draw(Matrix{}, rrect, paint);

  dynamic_rrect_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_rrect_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);

  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kRRect,
                                HWFragmentKeyType::kSolidVertex));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolidVertex,
                                HWGeometryKeyType::kRRect,
                                HWColorFilterKeyType::kLinearToSRGBGamma));

  EXPECT_EQ(steps[0]->GetVertexName(), "VS_RRect_SolidVertexColor");
  EXPECT_EQ(steps[0]->GetFragmentName(),
            "FS_SolidVertexColor_RRect_LinearToSRGBGammaFilter");
}

TEST(HWPipelineKey, RRect_SolidVertex_BlendSrcATop) {
  RRect rrect;
  Paint paint;
  paint.SetColorFilter(ColorFilters::Blend(0xFF00FF00, BlendMode::kSrcATop));
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicRRectDraw dynamic_rrect_draw(Matrix{}, rrect, paint);

  dynamic_rrect_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_rrect_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kRRect,
                                HWFragmentKeyType::kSolidVertex));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolidVertex,
                                HWGeometryKeyType::kRRect,
                                HWColorFilterKeyType::kSrcATop));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_RRect_SolidVertexColor");
  EXPECT_EQ(steps[0]->GetFragmentName(),
            "FS_SolidVertexColor_RRect_BlendSrcATopFilter");
}

TEST(HWPipelineKey, RRect_SolidVertex_Compose) {
  RRect rrect;
  Paint paint;
  paint.SetColorFilter(ColorFilters::Compose(
      ColorFilters::Blend(0xFF00FF00, BlendMode::kSrcATop),
      ColorFilters::Blend(0xFFFF0000, BlendMode::kSrcIn)));
  ArenaAllocator arena_allocator;
  HWDrawContext draw_context;
  draw_context.arena_allocator = &arena_allocator;
  bool is_stroke = false;
  bool use_gpu_tessellation = true;
  skity::HWDynamicRRectDraw dynamic_rrect_draw(Matrix{}, rrect, paint);

  dynamic_rrect_draw.Prepare(&draw_context);
  const ArrayList<HWDrawStep*, 2>& steps = dynamic_rrect_draw.GetSteps();
  EXPECT_EQ(steps.size(), 1);
  EXPECT_EQ(steps[0]->GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kRRect,
                                HWFragmentKeyType::kSolidVertex));
  EXPECT_EQ(steps[0]->GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kSolidVertex,
                                HWGeometryKeyType::kRRect,
                                HWColorFilterKeyType::kCompose));
  EXPECT_EQ(steps[0]->GetVertexName(), "VS_RRect_SolidVertexColor");
  EXPECT_EQ(steps[0]->GetFragmentName(),
            "FS_SolidVertexColor_RRect_ComposeFilter_BlendSrcInFilter_"
            "BlendSrcATopFilter");
  auto pipeline_key = steps[0]->GetPipelineKey();
  ASSERT_TRUE(pipeline_key.compose_keys.has_value());
  EXPECT_EQ(pipeline_key.compose_keys.value().size(), 2);
  EXPECT_EQ(pipeline_key.compose_keys.value()[0], HWColorFilterKeyType::kSrcIn);
  EXPECT_EQ(pipeline_key.compose_keys.value()[1],
            HWColorFilterKeyType::kSrcATop);
  HWPipelineKey expected_pipeline_key;
  expected_pipeline_key.base_key =
      MakePipelineBaseKey(MakeFunctionBaseKey(HWGeometryKeyType::kRRect,
                                              HWFragmentKeyType::kSolidVertex),
                          MakeFunctionBaseKey(HWFragmentKeyType::kSolidVertex,
                                              HWGeometryKeyType::kRRect,
                                              HWColorFilterKeyType::kCompose));
  expected_pipeline_key.compose_keys = {HWColorFilterKeyType::kSrcIn,
                                        HWColorFilterKeyType::kSrcATop};
  EXPECT_EQ(pipeline_key, expected_pipeline_key);
  HWPipelineKeyHash pipeline_key_hash;
  EXPECT_EQ(pipeline_key_hash(pipeline_key),
            pipeline_key_hash(expected_pipeline_key));
}

TEST(HWPipelineKey, Text) {
  {
    auto geometry = WGSLTextSolidColorGeometry(Matrix{}, {}, Paint{});
    EXPECT_EQ(geometry.GetMainKey(), HWGeometryKeyType::kColorText);
    EXPECT_EQ(geometry.GetShaderName(), "TextSolidColorVertexWGSL");
  }
  {
    auto geometry = WGSLTextGradientGeometry({}, {}, {}, {});
    EXPECT_EQ(geometry.GetMainKey(), HWGeometryKeyType::kGradientText);
    EXPECT_EQ(geometry.GetShaderName(), "TextGradientVertexWGSL");
  }
  {
    auto fragment = WGSLColorTextFragment({}, nullptr);

    EXPECT_EQ(fragment.GetMainKey(), HWFragmentKeyType::kColorText);
    EXPECT_EQ(fragment.GetShaderName(), "ColorTextFragmentWGSL");
  }
  {
    bool swizzle_rb = false;
    auto fragment1 = WGSLColorEmojiFragment({}, {}, swizzle_rb, 0);
    EXPECT_EQ(fragment1.GetMainKey(), HWFragmentKeyType::kEmojiText);
    EXPECT_EQ(fragment1.GetShaderName(), "ColorEmojiNoSwizzleFragmentWGSL");

    swizzle_rb = true;
    auto fragment2 = WGSLColorEmojiFragment({}, {}, swizzle_rb, 0);
    EXPECT_EQ(fragment2.GetMainKey(), HWFragmentKeyType::kEmojiText | (1 << 8));
    EXPECT_EQ(fragment2.GetShaderName(), "ColorEmojiSwizzleRBFragmentWGSL");
  }
  {
    auto geometry = WGSLTextSolidColorGeometry(Matrix{}, {}, Paint{});
    EXPECT_EQ(geometry.GetMainKey(), HWGeometryKeyType::kColorText);
    EXPECT_EQ(geometry.GetShaderName(), "TextSolidColorVertexWGSL");

    auto fragment = WGSLSdfColorTextFragment({}, nullptr, {});
    EXPECT_EQ(fragment.GetMainKey(), HWFragmentKeyType::kSDFText);
    EXPECT_EQ(fragment.GetShaderName(), "SdfColorTextFragmentWGSL");
    auto step = ColorStep(&geometry, &fragment, CoverageType::kNone);
    EXPECT_EQ(step.GetVertexKey(),
              MakeFunctionBaseKey(HWGeometryKeyType::kColorText));
    EXPECT_EQ(step.GetFragmentKey(),
              MakeFunctionBaseKey(HWFragmentKeyType::kSDFText));

    auto cf = ColorFilters::Blend(0xFF00FF00, BlendMode::kSrcATop);
    fragment.SetFilter(WGXFilterFragment::Make(cf.get()));
    EXPECT_EQ(fragment.GetMainKey(), HWFragmentKeyType::kSDFText);
    EXPECT_EQ(fragment.GetShaderName(),
              "SdfColorTextFragmentWGSL_BlendSrcATopFilter");
    step = ColorStep(&geometry, &fragment, CoverageType::kNone);
    EXPECT_EQ(step.GetVertexKey(),
              MakeFunctionBaseKey(HWGeometryKeyType::kColorText));
    EXPECT_EQ(step.GetFragmentKey(),
              MakeFunctionBaseKey(HWFragmentKeyType::kSDFText, 0,
                                  HWColorFilterKeyType::kSrcATop));
  }
  {
    Color4f colors[2] = {Color4f{0, 0, 0, 0}, Color4f{1, 1, 1, 1}};
    float pos[2] = {0, 1};
    auto shader = LinearGradientShader::MakeSweep(0, 0, 0, 90, colors, pos, 2);
    skity::Shader::GradientInfo info;
    auto type = shader->AsGradient(&info);
    auto fragment = WGSLGradientTextFragment({}, {}, info, type, 0);
    EXPECT_EQ(fragment.GetMainKey(),
              MakeMainKey(HWFragmentKeyType::kGradientText, 0b10001100));
    EXPECT_EQ(fragment.GetShaderName(), "GradientSweep2ColorFastTextWGSL");
  }
}

TEST(HWPipelineKey, BlurFilter) {
  auto geometry = WGSLFilterGeometry(1.0f, 1.0f);
  auto fragment = WGSLBlurFilter({}, {}, 0.0f, {}, {});
  auto step = ColorStep(&geometry, &fragment, CoverageType::kNone);

  EXPECT_EQ(step.GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kFilter));
  EXPECT_EQ(step.GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kBlur));
  EXPECT_EQ(step.GetVertexName(), "CommonFilterVertexWGSL");
  EXPECT_EQ(step.GetFragmentName(), "BlurFragmentWGSL");
}

TEST(HWPipelineKey, Texture) {
  const std::vector<BatchGroup<RRect>> rrects = {};
  auto geometry = WGSLRRectGeometry(rrects);
  std::shared_ptr<skity::Pixmap> pixmap = std::make_shared<skity::Pixmap>(
      500, 500, skity::AlphaType::kUnpremul_AlphaType, skity::ColorType::kRGBA);
  auto image = skity::Image::MakeImage(pixmap);
  auto shader = std::make_shared<skity::PixmapShader>(
      image, skity::SamplingOptions{}, skity::TileMode::kClamp,
      skity::TileMode::kClamp, skity::Matrix{});
  skity::WGSLTextureFragment fragment{shader,          nullptr, nullptr, 1.0f,
                                      skity::Matrix{}, 500,     500};
  auto step = ColorStep(&geometry, &fragment, CoverageType::kNone);

  EXPECT_EQ(step.GetVertexKey(),
            MakeFunctionBaseKey(HWGeometryKeyType::kRRect,
                                HWFragmentKeyType::kTexture));
  EXPECT_EQ(step.GetFragmentKey(),
            MakeFunctionBaseKey(HWFragmentKeyType::kTexture,
                                HWGeometryKeyType::kRRect));
  EXPECT_EQ(step.GetVertexName(), "VS_RRect_Texture");
  EXPECT_EQ(step.GetFragmentName(), "FS_Texture_RRect");
}
