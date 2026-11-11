// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>
#include <skity/render/precompile_context.hpp>
#include <skity/text/text_blob.hpp>
#include <string>
#include <utility>
#include <vector>

#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_coverage_path_draw.hpp"
#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/hw_render_pass_builder.hpp"
#include "test/ut/gpu/fake_gpu_context.hpp"

namespace skity {
namespace {

using test::FakeGPUContext;
using test::FakeGPUSurface;
using test::FakeRenderPass;

std::unique_ptr<PrecompileContext> MakePrecompileContext(
    FakeGPUContext& context, bool enable_msaa) {
  return context.CreatePrecompileContext(PrecompileColorType::kRGBA,
                                         enable_msaa);
}

void PrecompileDefaultShaders(FakeGPUContext& context, bool enable_msaa) {
  MakePrecompileContext(context, enable_msaa)->PrecompileDefaultShaders();
}

void PrecompileDraw(FakeGPUContext& context, bool enable_msaa,
                    PrecompileDrawType draw_type, const Paint& paint) {
  MakePrecompileContext(context, enable_msaa)->PrecompileDraw(draw_type, paint);
}

Path MakeTestPath() {
  Path path;
  path.MoveTo(0.f, 0.f);
  path.LineTo(16.f, 0.f);
  path.LineTo(0.f, 16.f);
  path.LineTo(16.f, 16.f);
  path.Close();
  return path;
}

Path MakeConvexTestPath() {
  Path path;
  path.MoveTo(2.f, 2.f);
  path.LineTo(24.f, 4.f);
  path.LineTo(8.f, 26.f);
  path.Close();
  return path;
}

std::shared_ptr<Image> MakeTestImage() {
  auto pixmap = std::make_shared<Pixmap>(1, 1, AlphaType::kPremul_AlphaType,
                                         ColorType::kRGBA);
  return Image::MakeImage(std::move(pixmap));
}

Paint MakeImagePaint(const std::shared_ptr<Image>& image) {
  Paint paint;
  paint.SetShader(Shader::MakeShader(image, SamplingOptions{}, TileMode::kDecal,
                                     TileMode::kDecal, Matrix{}));
  return paint;
}

template <typename DrawProc>
void ExpectRealDrawHitsPrecompiledPipeline(FakeGPUContext& context,
                                           PrecompileDrawType draw_type,
                                           const Paint& paint, bool enable_msaa,
                                           DrawProc draw_proc) {
  auto precompile_context = MakePrecompileContext(context, enable_msaa);
  precompile_context->PrecompileDraw(draw_type, paint);

  auto* device = context.device();
  ASSERT_NE(device, nullptr);
  device->set_disallow_shader_pipeline_creation(true);

  GPUSurfaceDescriptor desc{};
  desc.width = 32;
  desc.height = 32;
  desc.sample_count = enable_msaa ? 4 : 1;
  auto surface = context.CreateSurface(&desc);
  auto* canvas = surface->LockCanvas();
  draw_proc(canvas);
  canvas->Flush();
  surface->Flush();

  EXPECT_EQ(device->disallowed_shader_function_count(), 0u);
  EXPECT_EQ(device->disallowed_render_pipeline_count(), 0u);
  EXPECT_EQ(device->disallowed_clone_pipeline_count(), 0u);

  device->set_disallow_shader_pipeline_creation(false);
}

}  // namespace

TEST(InitialColorClear, ConsecutiveClearsAndSplitPass) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  GPUSurfaceDescriptor desc{};
  desc.width = desc.height = 32;
  FakeGPUSurface surface(desc, &context, GPUTextureFormat::kRGBA8Unorm, true);
  for (bool split : {false, true}) {
    auto* canvas = surface.LockCanvas(false);
    // Inverse-mapping and mapping back would round the right edge below 32.
    canvas->Save();
    canvas->Translate(0.5f, 0);
    canvas->Scale(1.1f, 1.f);
    canvas->Clear(Color_WHITE);
    canvas->Restore();
    canvas->Save();
    canvas->Rotate(35, 16, 16);
    canvas->Clear(Color4f{0.5f, 1.f, 0.f, 0.5f});
    canvas->Restore();
    if (split) {
      Paint paint;
      paint.SetBlendMode(BlendMode::kOverlay);
      canvas->DrawRect(Rect::MakeWH(8, 8), paint);
    }
    canvas->Flush();
    surface.Flush();
    const auto& colors = context.device()->last_colors();
    ASSERT_EQ(colors.size(), split ? 2u : 1u);
    EXPECT_EQ(colors[0].load_op, GPULoadOp::kClear);
    EXPECT_EQ(colors[0].clear_value.r, 0.25);
    EXPECT_EQ(colors[0].clear_value.g, 0.5);
    EXPECT_EQ(colors[0].clear_value.b, 0);
    EXPECT_EQ(colors[0].clear_value.a, 0.5);
    if (split) {
      EXPECT_EQ(colors[1].load_op, GPULoadOp::kLoad);
    } else {
      EXPECT_TRUE(context.device()->last_instance_counts().empty());
    }
  }
}

TEST(InitialColorClear, ClearBlendProducesTransparentBlackForThisFrameOnly) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  GPUSurfaceDescriptor desc{};
  desc.width = desc.height = 32;
  FakeGPUSurface surface(desc, &context, GPUTextureFormat::kRGBA8Unorm, true);
  auto* canvas = surface.LockCanvas(false);
  canvas->DrawColor(Color_RED, BlendMode::kClear);
  canvas->Flush();
  surface.Flush();

  auto colors = context.device()->last_colors();
  ASSERT_EQ(colors.size(), 1u);
  EXPECT_EQ(colors[0].load_op, GPULoadOp::kClear);
  EXPECT_EQ(colors[0].clear_value.r, 0.);
  EXPECT_EQ(colors[0].clear_value.g, 0.);
  EXPECT_EQ(colors[0].clear_value.b, 0.);
  EXPECT_EQ(colors[0].clear_value.a, 0.);
  EXPECT_TRUE(context.device()->last_instance_counts().empty());

  canvas = surface.LockCanvas(false);
  canvas->Flush();
  surface.Flush();
  ASSERT_EQ(context.device()->last_colors().size(), 1u);
  EXPECT_EQ(context.device()->last_colors()[0].load_op, GPULoadOp::kLoad);
}

TEST(InitialColorClear, OversizedTransformedRectAfterRestoredClip) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  GPUSurfaceDescriptor desc{};
  desc.width = desc.height = 32;
  desc.content_scale = 1.5f;
  desc.sample_count = 4;
  FakeGPUSurface surface(desc, &context, GPUTextureFormat::kRGBA8Unorm, true);
  auto* canvas = surface.LockCanvas(true);
  canvas->Save();
  canvas->ClipPath(MakeTestPath());
  canvas->Restore();
  canvas->Translate(32, 0);
  canvas->Scale(-2, 2);
  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(Color_RED);
  canvas->DrawRect(Rect::MakeLTRB(-1, -1, 17, 17), paint);
  canvas->Flush();
  surface.Flush();
  const auto& colors = context.device()->last_colors();
  ASSERT_EQ(colors.size(), 1u);
  EXPECT_EQ(colors[0].load_op, GPULoadOp::kClear);
  EXPECT_EQ(colors[0].clear_value.r, 1.);
  EXPECT_TRUE(context.device()->last_instance_counts().empty());
}

TEST(InitialColorClear, FullRectPathReplacesRetainedColor) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  GPUSurfaceDescriptor desc{};
  desc.width = desc.height = 32;
  FakeGPUSurface surface(desc, &context, GPUTextureFormat::kRGBA8Unorm, true);
  auto* canvas = surface.LockCanvas(false);
  Path path;
  path.AddRect(Rect::MakeWH(32, 32));
  Paint paint;
  paint.SetBlendMode(BlendMode::kSrc);
  paint.SetFillColor(Color4f{1, 0, 0, 0.5f});
  canvas->DrawPath(path, paint);
  canvas->Flush();
  surface.Flush();
  const auto& colors = context.device()->last_colors();
  ASSERT_EQ(colors.size(), 1u);
  EXPECT_EQ(colors[0].load_op, GPULoadOp::kClear);
  EXPECT_EQ(colors[0].clear_value.r, 0.5);
  EXPECT_EQ(colors[0].clear_value.a, 0.5);
  EXPECT_TRUE(context.device()->last_instance_counts().empty());
}

TEST(InitialColorClear, RetainsDestinationWhenPromotionIsUnsafe) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto expect_load = [&](auto draw) {
    GPUSurfaceDescriptor desc{};
    desc.width = desc.height = 32;
    FakeGPUSurface surface(desc, &context, GPUTextureFormat::kRGBA8Unorm, true);
    auto* canvas = surface.LockCanvas(false);
    draw(canvas);
    canvas->Flush();
    surface.Flush();
    const auto& colors = context.device()->last_colors();
    ASSERT_FALSE(colors.empty());
    EXPECT_EQ(colors.front().load_op, GPULoadOp::kLoad);
    EXPECT_FALSE(context.device()->last_instance_counts().empty());
  };
  expect_load([](Canvas* c) {
    Paint p;
    p.SetAntiAlias(true);
    c->DrawRect(Rect::MakeWH(31.99f, 32), p);
  });
  expect_load([](Canvas* c) { c->DrawColor(Color4f{1, 0, 0, 0.9999f}); });
  expect_load([](Canvas* c) {
    c->DrawRect(Rect::MakeWH(8, 8), Paint{});
    c->Clear(Color_RED);
  });
  expect_load([](Canvas* c) {
    c->ClipRect(Rect::MakeWH(16, 16));
    c->Clear(Color_RED);
  });
  expect_load([](Canvas* c) {
    c->ClipPath(MakeTestPath(), Canvas::ClipOp::kDifference);
    c->Clear(Color_RED);
  });
  expect_load([](Canvas* c) {
    Paint p;
    p.SetPathEffect(PathEffect::MakeDiscretePathEffect(4, 2));
    c->DrawRect(Rect::MakeWH(32, 32), p);
  });
  expect_load([](Canvas* c) {
    c->Rotate(45, 16, 16);
    c->DrawRect(Rect::MakeWH(32, 32), Paint{});
  });
  expect_load([](Canvas* c) { c->DrawPaint(MakeImagePaint(MakeTestImage())); });
  expect_load([](Canvas* c) {
    c->SaveLayer(Rect::MakeWH(32, 32), Paint{});
    c->Clear(Color_RED);
    c->Restore();
  });
  expect_load([](Canvas* c) {
    Path path;
    path.SetFillType(Path::PathFillType::kEvenOdd);
    path.AddRect(Rect::MakeWH(32, 32));
    path.AddRect(Rect::MakeLTRB(8, 8, 24, 24));
    c->DrawPath(path, Paint{});
  });
  expect_load([](Canvas* c) {
    Path path;
    path.MoveTo(0, 0);
    path.Close();
    path.AddRect(Rect::MakeLTRB(8, 8, 32, 32));
    c->DrawPath(path, Paint{});
  });
}

TEST(PrecompileDrawTest, ReturnsBeforeContextInit) {
  FakeGPUContext context;
  Paint paint;

  PrecompileDefaultShaders(context, false);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);
}

TEST(PrecompileDrawTest, InvalidPrecompileColorTypeDoesNotCreateGPUResources) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  EXPECT_FALSE(context.IsForceDepthStencilPipelineState());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  auto invalid_context = context.CreatePrecompileContext(
      static_cast<PrecompileColorType>(255), false);

  Paint paint;
  invalid_context->PrecompileDefaultShaders();
  invalid_context->PrecompileDraw(PrecompileDrawType::kDrawImage, paint);

  EXPECT_FALSE(context.IsForceDepthStencilPipelineState());
  EXPECT_EQ(device->shader_function_count(), 0u);
  EXPECT_EQ(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, PrecompileContextForcesDepthStencilLayerState) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  EXPECT_FALSE(context.IsForceDepthStencilPipelineState());

  auto precompile_context = MakePrecompileContext(context, false);
  ASSERT_NE(precompile_context, nullptr);
  EXPECT_TRUE(context.IsForceDepthStencilPipelineState());

  GPUSurfaceDescriptor desc{};
  desc.width = 32;
  desc.height = 32;
  auto surface = context.CreateSurface(&desc);
  auto* fake_surface = static_cast<FakeGPUSurface*>(surface.get());

  Paint paint;
  auto* canvas = surface->LockCanvas();
  canvas->DrawRect(Rect::MakeWH(16.f, 16.f), paint);
  canvas->Flush();
  surface->Flush();

  EXPECT_EQ(fake_surface->last_draw_state(),
            kDrawStateDepth | kDrawStateStencil);
}

TEST(PrecompileDrawTest, PrecompileDefaultShadersSupportsCommonDraws) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  PrecompileDefaultShaders(context, false);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDefaultShaders(context, false);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompileDefaultShadersCoversExplicitDefaultDraws) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);

  PrecompileDefaultShaders(context, false);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImageRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompileDefaultShadersCoversGPUTessellationPath) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  PrecompileDefaultShaders(context, false);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
}

TEST(PrecompileDrawTest,
     PrecompileDefaultShadersCoversTwoColorLinearGradientPath) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  PrecompileDefaultShaders(context, false);

  EXPECT_TRUE(device->HasFragmentFunctionLabel(
      "FS_GradientLinear2OffsetFastColorFast"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel("FS_GradientLinear2ColorFast"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel("FS_GradientLinear2"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  Paint paint;
  Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {32.f, 32.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, nullptr, 2));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  float color_fast_pos[] = {0.f, 1.f};
  paint.SetShader(Shader::MakeLinear(pts, colors, color_fast_pos, 2));
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  float pos[] = {0.25f, 0.75f};
  paint.SetShader(Shader::MakeLinear(pts, colors, pos, 2));
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompilePathUsesContourAAWhenEnabled) {
  for (auto style : {Paint::kFill_Style, Paint::kStroke_Style}) {
    SCOPED_TRACE(style);
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    context.SetEnableContourAA(true);
    auto* device = context.device();

    Paint paint;
    paint.SetStyle(style);
    paint.SetStrokeWidth(4.f);
    paint.SetAntiAlias(true);
    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawPath, paint, false,
        [&](Canvas* canvas) { canvas->DrawPath(MakeTestPath(), paint); });

    EXPECT_TRUE(device->HasVertexFunctionLabelContaining("PathAA"));
    EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
    EXPECT_EQ(device->last_instance_counts().size(), 3u);
  }
}

TEST(PrecompileDrawTest, AnalyticalAAModesCanBeEnabledTogether) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());

  context.SetEnableContourAA(true);
  EXPECT_TRUE(context.IsEnableContourAA());
  EXPECT_FALSE(context.IsEnableCoverageAA());

  context.SetEnableCoverageAA(true);
  EXPECT_TRUE(context.IsEnableCoverageAA());
  EXPECT_TRUE(context.IsEnableContourAA());
}

TEST(PrecompileDrawTest, AnalyticalAAModesAreMutuallyExclusiveInsideCanvas) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.0f;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Matrix perspective;
  perspective.SetPersp0(0.001f);
  canvas->Concat(perspective);

  Path path;
  path.MoveTo(8, 8);
  path.LineTo(56, 8);
  path.LineTo(8, 56);
  path.Close();

  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPath"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 0u);
}

TEST(PrecompileDrawTest,
     SurfaceAnalyticalModeOverridesContextConflationCorrection) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetConflationCorrection(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.0f;
  surface_desc.render_options.coverage_aa = CoverageAAMode::kAnalytical;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.MoveTo(16, 16);
  path.LineTo(48, 16);
  path.LineTo(48, 48);
  path.LineTo(16, 48);
  path.Close();
  path.SetFillType(Path::PathFillType::kEvenOdd);

  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);

  Path second_path;
  second_path.MoveTo(8, 8);
  second_path.LineTo(24, 8);
  second_path.LineTo(8, 24);
  second_path.Close();
  second_path.SetFillType(Path::PathFillType::kEvenOdd);
  canvas->DrawPath(second_path, paint);

  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAAFill"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("CoverageAAFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining(
      "CoverageAAConflationCorrection"));
  EXPECT_EQ(device->last_render_pass_count(), 1u);
}

TEST(PrecompileDrawTest, SurfaceCanDisableCoverageAADefault) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.render_options.coverage_aa = CoverageAAMode::kDisabled;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8.25f, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 0u);
}

TEST(PrecompileDrawTest, SurfaceCanEnableCoverageAAWithoutContextDefault) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.render_options.coverage_aa = CoverageAAMode::kAnalytical;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8.25f, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 1u);
}

TEST(PrecompileDrawTest, FallsBackWhenFragmentMaskBlendIsUnsupported) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  auto surface = context.CreateSurface(&surface_desc);
  static_cast<FakeGPUSurface*>(surface.get())
      ->SetSupportsTextureCopyDstRead(false);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(0x80FFFFFF);
  paint.SetBlendMode(BlendMode::kSrc);
  canvas->DrawRRect(RRect::MakeRectXY(Rect::MakeLTRB(8, 8, 56, 56), 8.f, 8.f),
                    paint);
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("Path"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 0u);
}

TEST(PrecompileDrawTest, SurfaceCanSelectConflationCorrection) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.render_options.coverage_aa =
      CoverageAAMode::kConflationCorrection;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8.25f, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining(
      "CoverageAAConflationCorrection"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 1u);
}

TEST(PrecompileDrawTest, RenderTargetUsesSurfaceCoverageAAOptions) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetConflationCorrection(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPURenderTargetDescriptor target_desc{};
  target_desc.width = 64;
  target_desc.height = 64;
  target_desc.render_options.coverage_aa = CoverageAAMode::kAnalytical;
  auto target = context.CreateRenderTarget(target_desc);
  ASSERT_NE(target, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8.25f, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  target->GetCanvas()->DrawPath(path, paint);

  ASSERT_NE(context.MakeSnapshot(std::move(target)), nullptr);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining(
      "CoverageAAConflationCorrection"));
}

TEST(PrecompileDrawTest, CoverageAADoesNotAddLayerMaskPasses) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 96;
  surface_desc.height = 96;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.0f;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.MoveTo(8, 8);
  path.LineTo(40, 8);
  path.LineTo(8, 40);
  path.Close();

  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);

  canvas->SaveLayer(Rect::MakeWH(64, 64), Paint{});
  canvas->DrawPath(path, paint);
  canvas->Restore();
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAAFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_EQ(device->last_render_pass_count(), 2u);
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 1u);
}

TEST(PrecompileDrawTest, PrecompilePathUsesGPUTessellationWithMSAA) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  paint.SetAntiAlias(true);
  PrecompileDraw(context, true, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompilePathUsesGPUTessellationWithoutContourAA) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  paint.SetAntiAlias(true);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompilePathUsesGPUTessellationWhenPaintDisablesAA) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  paint.SetAntiAlias(false);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompilePathUsesPathWhenGPUTessellationDisabled) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableGPUTessellation(false);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("VS_Path"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompileRRectUsesSolidVertexColor) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("SolidVertexColor"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
}

TEST(PrecompileDrawTest, PrecompileStrokeRectUsesRRectPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStroke_Style);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRect, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("SolidVertexColor"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
}

TEST(PrecompileDrawTest, PrecompileDrawIncludesColorFilterPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetColorFilter(ColorFilters::Blend(0xFF00FF00, BlendMode::kSrcATop));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("BlendSrcATopFilter"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompileDrawIncludesAdvancedBlendingPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  paint.SetBlendMode(BlendMode::kOverlay);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_GT(device->shader_function_count(), shader_function_count);
  EXPECT_GT(device->render_pipeline_count(), render_pipeline_count);

  shader_function_count = device->shader_function_count();
  render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest,
     PrecompileRRectFallsBackToPathWhenSimpleShapeDisabled) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("RRect"));
}

TEST(PrecompileDrawTest, CreatesAndCachesSolidDrawPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();
  auto clone_pipeline_count = device->clone_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
  EXPECT_EQ(device->clone_pipeline_count(), clone_pipeline_count);

  PrecompileDraw(context, true, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
  EXPECT_GT(device->clone_pipeline_count(), 0u);
}

TEST(PrecompileDrawTest, StrokeWritesColorAndDepthInOneCommand) {
  for (bool gpu_tessellation : {false, true}) {
    for (uint32_t samples : {1u, 4u}) {
      SCOPED_TRACE(::testing::Message()
                   << "gpu=" << gpu_tessellation << " samples=" << samples);
      FakeGPUContext context;
      ASSERT_TRUE(context.Init());
      ArenaAllocator arena;
      HWStageBuffer stage_buffer(context.device());
      HWStaticBuffer static_buffer(context.device());
      VectorCache<float> vertices;
      VectorCache<uint32_t> indices;
      HWDrawContext draw_context{};
      draw_context.arena_allocator = &arena;
      draw_context.gpuContext = &context;
      draw_context.pipelineLib = context.GetPipelineLib();
      draw_context.stageBuffer = &stage_buffer;
      draw_context.static_buffer = &static_buffer;
      draw_context.vertex_vector_cache = &vertices;
      draw_context.index_vector_cache = &indices;
      draw_context.total_clip_depth = 4;

      Paint paint;
      paint.SetStyle(Paint::kStroke_Style);
      paint.SetStrokeWidth(8.f);
      paint.SetAlphaF(0.5f);
      HWDynamicPathDraw draw(Matrix{}, MakeTestPath(), paint, true,
                             gpu_tessellation);
      draw.SetClipDepth(2);
      draw.SetSampleCount(samples);
      draw.SetScissorBox(Rect::MakeWH(32.f, 32.f));
      auto plan = ResolveHWBlendPlan(BlendMode::kSrcOver, false, false,
                                     context.device()->GetCaps(), true);
      ASSERT_TRUE(plan.has_value());
      draw.SetBlendPlan(*plan);
      auto state = draw.Prepare(&draw_context);
      ASSERT_EQ(state, kDrawStateDepth | kDrawStateStencil);
      EXPECT_FLOAT_EQ(draw.GetClipValue(), 0.4f);
      draw.GenerateCommand(&draw_context, state);
      std::vector<uint32_t> instance_counts;
      FakeRenderPass pass(GPURenderPassDescriptor{}, instance_counts);
      draw.Draw(&pass, nullptr);
      ASSERT_EQ(pass.GetCommands().size(), 1u);
      const auto* command = pass.GetCommands().front();
      ASSERT_GT(command->index_count, 0u);
      const auto& pipeline = command->pipeline->GetDescriptor();
      EXPECT_EQ(pipeline.sample_count, samples);
      EXPECT_EQ(pipeline.target.write_mask, 0xFu);
      EXPECT_EQ(pipeline.depth_stencil.format,
                GPUTextureFormat::kDepth24Stencil8);
      EXPECT_TRUE(pipeline.depth_stencil.enable_depth);
      EXPECT_TRUE(pipeline.depth_stencil.depth_state.enableWrite);
      EXPECT_EQ(pipeline.depth_stencil.depth_state.compare,
                GPUCompareFunction::kGreater);
      EXPECT_FALSE(pipeline.depth_stencil.enable_stencil);
    }
  }
}

TEST(PrecompileDrawTest, ColorStepDepthWriteIsIndependentOfCoverage) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  ArenaAllocator arena;
  HWStageBuffer stage_buffer(context.device());
  VectorCache<float> vertices;
  VectorCache<uint32_t> indices;
  HWDrawContext draw_context{};
  draw_context.arena_allocator = &arena;
  draw_context.gpuContext = &context;
  draw_context.pipelineLib = context.GetPipelineLib();
  draw_context.stageBuffer = &stage_buffer;
  draw_context.vertex_vector_cache = &vertices;
  draw_context.index_vector_cache = &indices;

  HWDrawStepContext step_context{};
  step_context.context = &draw_context;
  step_context.state = kDrawStateDepth | kDrawStateStencil;
  step_context.clip_depth = 0.5f;
  step_context.scissor = Rect::MakeWH(32.f, 32.f);

  Paint paint;
  auto path = MakeConvexTestPath();
  WGSLPathGeometry geometry(path, paint, false);
  WGSLSolidColor fragment(paint.GetFillColor());
  for (auto coverage :
       {CoverageType::kNone, CoverageType::kEvenOdd, CoverageType::kWinding}) {
    for (bool write_depth : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "coverage=" << static_cast<int>(coverage)
                   << " write_depth=" << write_depth);
      ColorStep step(&geometry, &fragment, coverage, write_depth);
      EXPECT_EQ(step.RequireDepth(), write_depth);
      EXPECT_EQ(step.RequireStencil(), coverage != CoverageType::kNone);
      Command command;
      step.GenerateCommand(step_context, &command, nullptr);
      ASSERT_TRUE(command.IsValid());
      const auto& state = command.pipeline->GetDescriptor().depth_stencil;
      EXPECT_TRUE(state.enable_depth);
      EXPECT_EQ(state.depth_state.enableWrite, write_depth);
      EXPECT_EQ(state.depth_state.compare, GPUCompareFunction::kGreater);
      EXPECT_EQ(state.enable_stencil, coverage != CoverageType::kNone);
      if (state.enable_stencil) {
        EXPECT_EQ(state.stencil_state.front, state.stencil_state.back);
        EXPECT_EQ(state.stencil_state.front.compare,
                  GPUCompareFunction::kNotEqual);
        EXPECT_EQ(state.stencil_state.front.pass_op,
                  GPUStencilOperation::kReplace);
        EXPECT_EQ(state.stencil_state.front.stencil_read_mask,
                  coverage == CoverageType::kEvenOdd ? 0x01u : 0xFFFFFFFFu);
      }
    }
  }
}

TEST(PrecompileDrawTest, RenderPassAttachmentsMatchDrawState) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  HWRenderTargetCache::Pool pool(context.GetRenderTargetCache());
  HWDrawContext draw_context{};
  draw_context.gpuContext = &context;
  draw_context.pool = &pool;
  GPUTextureDescriptor target_desc;
  target_desc.width = target_desc.height = 32;
  target_desc.format = GPUTextureFormat::kRGBA8Unorm;
  auto target = context.device()->CreateTexture(target_desc);
  for (auto state : {kDrawStateNone, kDrawStateStencil,
                     kDrawStateDepth | kDrawStateStencil}) {
    for (uint32_t samples : {1u, 4u}) {
      GPURenderPassDescriptor pass;
      HWRenderPassBuilder(&draw_context, target)
          .SetSampleCount(samples)
          .SetDrawState(state)
          .Build(pass);
      if (state == kDrawStateNone) {
        EXPECT_EQ(pass.depth_attachment.texture, nullptr);
        EXPECT_EQ(pass.stencil_attachment.texture, nullptr);
        continue;
      }
      ASSERT_NE(pass.stencil_attachment.texture, nullptr);
      const auto& attachment = pass.stencil_attachment.texture->GetDescriptor();
      EXPECT_EQ(attachment.sample_count, samples);
      if (state & kDrawStateDepth) {
        EXPECT_EQ(attachment.format, GPUTextureFormat::kDepth24Stencil8);
        EXPECT_EQ(pass.depth_attachment.texture,
                  pass.stencil_attachment.texture);
        EXPECT_EQ(pass.depth_attachment.load_op, GPULoadOp::kClear);
        EXPECT_EQ(pass.depth_attachment.store_op, GPUStoreOp::kDiscard);
        EXPECT_FLOAT_EQ(pass.depth_attachment.clear_value, 0.f);
      } else {
        EXPECT_EQ(attachment.format, GPUTextureFormat::kStencil8);
        EXPECT_EQ(pass.depth_attachment.texture, nullptr);
      }
    }
  }
}

TEST(PrecompileDrawTest, PrecompiledStrokePipelineIsHitByRealDraw) {
  for (bool gpu_tessellation : {false, true}) {
    for (bool msaa : {false, true}) {
      for (bool perspective : {false, true}) {
        SCOPED_TRACE(::testing::Message()
                     << "gpu=" << gpu_tessellation << " msaa=" << msaa
                     << " perspective=" << perspective);
        FakeGPUContext context;
        ASSERT_TRUE(context.Init());
        context.SetEnableGPUTessellation(gpu_tessellation);
        Paint paint;
        paint.SetStyle(Paint::kStroke_Style);
        paint.SetStrokeWidth(4.f);
        paint.SetAlphaF(0.5f);
        ExpectRealDrawHitsPrecompiledPipeline(
            context, PrecompileDrawType::kDrawPath, paint, msaa,
            [&](Canvas* canvas) {
              if (perspective) {
                Matrix transform;
                transform.SetPersp0(0.001f);
                canvas->Concat(transform);
              }
              canvas->DrawPath(MakeTestPath(), paint);
            });
        EXPECT_EQ(context.device()->last_instance_counts().size(), 1u);
        EXPECT_FALSE(
            context.device()->HasFragmentFunctionLabel("StencilFragmentWGSL"));
      }
    }
  }
}

TEST(PrecompileDrawTest, PrecompiledPathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawPath, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledConvexTessPathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  paint.SetAntiAlias(false);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawPath, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeConvexTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledConvexPathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableGPUTessellation(false);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  paint.SetAntiAlias(false);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawPath, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeConvexTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledRRectPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  auto rrect = RRect::MakeRectXY(Rect::MakeWH(24.f, 24.f), 4.f, 4.f);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawRRect, paint, false,
      [&](Canvas* canvas) { canvas->DrawRRect(rrect, paint); });
}

TEST(RectGeometry, SelectsFromPhysicalBoundsAndSampleCount) {
  const auto aligned = Rect::MakeLTRB(2, 2, 14, 14);
  const auto fractional = Rect::MakeLTRB(2.5f, 2.5f, 14, 14);
  const struct {
    Rect rect;
    float content_scale;
    uint32_t sample_count;
    bool anti_alias;
    bool uses_quad;
  } cases[] = {
      {aligned, 1.f, 1, true, true},     {fractional, 1.f, 1, true, false},
      {fractional, 1.f, 1, false, true}, {fractional, 1.f, 4, true, true},
      {aligned, 1.25f, 1, true, false},  {fractional, 2.f, 1, true, true},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(::testing::Message() << "left=" << test.rect.Left()
                                      << " content_scale=" << test.content_scale
                                      << " samples=" << test.sample_count
                                      << " AA=" << test.anti_alias);
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    context.SetEnableContourAA(true);

    GPUSurfaceDescriptor desc{};
    desc.width = desc.height = 32;
    desc.content_scale = test.content_scale;
    desc.sample_count = test.sample_count;
    auto surface = context.CreateSurface(&desc);
    auto* canvas = surface->LockCanvas();
    Paint paint;
    paint.SetAntiAlias(test.anti_alias);
    canvas->DrawRect(test.rect, paint);
    canvas->Flush();

    const auto* device = context.device();
    EXPECT_EQ(device->HasVertexFunctionLabelContaining("VS_Rect"),
              test.uses_quad);
    if (test.uses_quad) {
      EXPECT_EQ(device->last_index_counts(), std::vector<uint32_t>{6u});
      EXPECT_EQ(device->last_instance_counts(), std::vector<uint32_t>{1u});
    } else {
      EXPECT_TRUE(device->HasVertexFunctionLabelContaining("PathAA"));
    }
  }
}

TEST(RectGeometry, RectPathWithExtraContourKeepsPathGeometry) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  GPUSurfaceDescriptor desc{};
  desc.width = desc.height = 32;
  auto surface = context.CreateSurface(&desc);
  auto* canvas = surface->LockCanvas();
  Path path;
  path.MoveTo(1.f, 1.f);
  path.Close();
  path.AddRect(Rect::MakeLTRB(8, 8, 16, 16));
  ASSERT_EQ(path.GetIsAType(), Path::IsAType::kGeneral);
  canvas->DrawPath(path, Paint{});
  canvas->Flush();

  EXPECT_FALSE(context.device()->HasVertexFunctionLabelContaining("VS_Rect"));
  EXPECT_TRUE(context.device()->HasVertexFunctionLabelContaining("Path"));
}

TEST(RectGeometry, NonCoherentNativeBlendKeepsOverlappingDrawsSeparate) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto caps = std::make_unique<GPUCaps>(context.device()->GetCaps());
  caps->supports_native_advanced_blend = true;
  caps->supports_native_advanced_blend_coherent = false;
  context.device()->InitCaps(std::move(caps));

  GPUSurfaceDescriptor desc{};
  desc.width = desc.height = 32;
  auto surface = context.CreateSurface(&desc);
  auto* canvas = surface->LockCanvas();
  Paint paint;
  paint.SetBlendMode(BlendMode::kOverlay);
  canvas->DrawRect(Rect::MakeLTRB(1, 1, 8, 8), paint);
  canvas->DrawRect(Rect::MakeLTRB(4, 4, 12, 12), paint);
  canvas->DrawRect(Rect::MakeLTRB(16, 16, 24, 24), paint);
  canvas->Flush();

  EXPECT_EQ(context.device()->last_render_pass_count(), 1u);
  EXPECT_EQ(context.device()->last_instance_counts(),
            (std::vector<uint32_t>{1u, 2u}));
}

TEST(PrecompileDrawTest, PrecompiledRectPipelineIsHitByBatch) {
  for (bool msaa : {false, true}) {
    SCOPED_TRACE(msaa);
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    Paint paint;
    paint.SetAntiAlias(true);
    paint.SetColor(Color_RED);
    auto rect = Rect::MakeLTRB(msaa ? 1.25f : 1.f, 1.f, 9.f, 9.f);
    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawRect, paint, msaa,
        [&](Canvas* canvas) {
          canvas->DrawRect(rect, paint);
          Path source;
          source.AddRect(rect).MoveTo(0.f, 0.f);
          Path path;
          path.AddPath(source);
          Paint second = paint;
          second.SetColor(ColorSetARGB(128, 0, 0, 255));
          canvas->Translate(24.f, 5.f);
          canvas->Scale(-1.f, 2.f);
          canvas->DrawPath(path, second);
        });
    EXPECT_TRUE(context.device()->HasVertexFunctionLabelContaining("VS_Rect"));
    EXPECT_EQ(context.device()->last_index_counts(), std::vector<uint32_t>{6u});
    EXPECT_EQ(context.device()->last_instance_counts(),
              std::vector<uint32_t>{2u});
  }
}

TEST(PrecompileDrawTest, PrecompiledImageRectPipelinesAreHitByRealDraw) {
  for (auto type :
       {PrecompileDrawType::kDrawImage, PrecompileDrawType::kDrawRect}) {
    SCOPED_TRACE(static_cast<int>(type));
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    auto image = MakeTestImage();
    Paint paint;
    if (type == PrecompileDrawType::kDrawRect) {
      SamplingOptions sampling;
      sampling.cubic = {1.f / 3.f, 1.f / 3.f};
      paint.SetShader(Shader::MakeShader(image, sampling, TileMode::kRepeat,
                                         TileMode::kRepeat, Matrix{}));
    }
    ExpectRealDrawHitsPrecompiledPipeline(
        context, type, paint, false, [&](Canvas* canvas) {
          const Rect rect = Rect::MakeLTRB(2, 2, 18, 18);
          for (int i = 0; i < 2; ++i) {
            if (type == PrecompileDrawType::kDrawImage) {
              canvas->DrawImageRect(image, Rect::MakeWH(1, 1), rect,
                                    SamplingOptions{}, &paint);
            } else {
              canvas->DrawRect(rect, paint);
            }
          }
        });
    EXPECT_TRUE(
        context.device()->HasVertexFunctionLabelContaining("VS_Rect_Texture"));
    EXPECT_EQ(context.device()->last_instance_counts(),
              (std::vector<uint32_t>{1u, 1u}));
  }
}

TEST(SimpleShapeDrawTest, AffinePathsUseRRectPipelineAndMerge) {
  Path paths[3];
  const Rect bounds = Rect::MakeLTRB(-8.f, -6.f, 8.f, 6.f);
  paths[0].AddRect(bounds);
  paths[1].AddOval(bounds);
  paths[2].AddRoundRect(bounds, 2.f, 3.f);

  // Exact 90 degrees has zero diagonal entries but is still invertible.
  Matrix quarter_turn = Matrix::Scale(0.f, 0.f);
  quarter_turn.SetSkewX(-1.f);
  quarter_turn.SetSkewY(1.f);
  const Matrix transforms[] = {
      Matrix::RotateDeg(5.f), Matrix::RotateDeg(45.f), quarter_turn,
      Matrix::Skew(0.4f, -0.2f),
      Matrix::RotateDeg(30.f) * Matrix::Scale(-2.f, 0.5f)};
  for (size_t shape = 0; shape < 3; ++shape) {
    Path source = paths[shape];
    source.MoveTo(bounds.Left(), bounds.Top());
    Path appended;
    appended.AddPath(source);
    for (size_t transform = 0; transform < 5; ++transform) {
      for (auto style : {Paint::kFill_Style, Paint::kStroke_Style}) {
        if (shape == 0 && style == Paint::kFill_Style) continue;
        SCOPED_TRACE(::testing::Message()
                     << "shape=" << shape << " transform=" << transform
                     << " style=" << static_cast<int>(style));
        FakeGPUContext context;
        ASSERT_TRUE(context.Init());
        context.SetEnableSimpleShapePipeline(true);
        context.SetEnableGPUTessellation(false);
        context.SetEnableMergingDrawCall(true);
        GPUSurfaceDescriptor desc{};
        desc.width = 128;
        desc.height = 128;
        auto surface = context.CreateSurface(&desc);
        auto* canvas = surface->LockCanvas();
        Paint paint;
        paint.SetAntiAlias(true);
        paint.SetStyle(style);
        paint.SetStrokeWidth(2.f);
        for (int i = 0; i < 2; ++i) {
          canvas->SetMatrix(
              Matrix::Translate(32.f + i * 64.f, 32.f + i * 64.f) *
              transforms[transform]);
          canvas->DrawPath(i == 0 ? paths[shape] : appended, paint);
        }
        canvas->Flush();
        surface->Flush();
        EXPECT_TRUE(
            context.device()->HasVertexFunctionLabelContaining("RRect"));
        EXPECT_FALSE(
            context.device()->HasVertexFunctionLabelContaining("Path"));
        EXPECT_EQ(context.device()->last_instance_counts(),
                  std::vector<uint32_t>{2u});
      }
    }
  }
}

TEST(PrecompileDrawTest,
     PrecompiledOpaqueAndTranslucentRRectPipelinesAreHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);

  auto rrect = RRect::MakeRectXY(Rect::MakeWH(24.f, 24.f), 4.f, 4.f);
  for (auto color : {0xFFFFFFFF, 0x80FFFFFF}) {
    Paint paint;
    paint.SetColor(color);
    paint.SetBlendMode(BlendMode::kSrc);
    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawRRect, paint, false,
        [&](Canvas* canvas) { canvas->DrawRRect(rrect, paint); });
  }
}

TEST(PrecompileDrawTest, PrecompiledImagePathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableGPUTessellation(false);

  auto image = MakeTestImage();
  auto paint = MakeImagePaint(image);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawImage, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeConvexTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledImageRRectPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);

  auto image = MakeTestImage();
  auto paint = MakeImagePaint(image);
  auto rrect = RRect::MakeRectXY(Rect::MakeWH(24.f, 24.f), 4.f, 4.f);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawImageRRect, paint, false,
      [&](Canvas* canvas) { canvas->DrawRRect(rrect, paint); });
}

TEST(PrecompileDrawTest, ClearsFailedPipelineCacheAfterPrecompileFailure) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  device->set_fail_render_pipeline_creation(true);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_EQ(device->render_pipeline_count(), 2u);

  device->set_fail_render_pipeline_creation(false);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_EQ(device->render_pipeline_count(), 4u);
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("VS_Rect"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("VS_Path"));
}

TEST(PrecompileDrawTest, ImagePrecompileDoesNotCreateTextureResources) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  device->set_fail_texture_creation(true);

  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("Texture"));
}

TEST(PrecompileDrawTest, ImagePrecompileUsesTexturePipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("Texture"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Solid"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("Path"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, ImageRRectPrecompileUsesTextureRRectPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImageRRect, paint);

  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("Texture"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsCommonSolidDrawTypes) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRect, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImageRRect, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kClipPath, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, TextPaintShadersHitPrecompiledPipelines) {
  auto typeface = Typeface::MakeFromFile(SKITY_FONT_DIR
                                         "fonts/resources/Roboto-Regular.ttf");
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph = typeface->UnicharToGlyph('O');
  ASSERT_NE(glyph, 0);
  std::vector<TextRun> runs;
  runs.emplace_back(Font(typeface, 16.f), std::vector<GlyphID>{glyph},
                    std::vector<float>{0.f}, std::vector<float>{0.f});
  TextBlob blob(std::move(runs));
  Paint gradient_paint;
  Point points[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  gradient_paint.SetShader(Shader::MakeLinear(points, colors, nullptr, 2));
  const Paint paints[] = {gradient_paint, MakeImagePaint(MakeTestImage())};
  for (const auto& paint : paints) {
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawText, paint, false,
        [&](Canvas* canvas) { canvas->DrawTextBlob(&blob, 4.f, 24.f, paint); });
    EXPECT_EQ(context.device()->last_instance_counts(),
              std::vector<uint32_t>{1u});
  }
}

TEST(PrecompileDrawTest, EmojiPaintShadersHitPrecompiledPipelines) {
  auto typeface = Typeface::MakeFromFile(SKITY_FONT_DIR
                                         "fonts/resources/NotoColorEmoji.ttf");
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph = typeface->UnicharToGlyph(0x1F600);
  ASSERT_NE(glyph, 0);
  std::vector<TextRun> runs;
  runs.emplace_back(Font(typeface, 16.f), std::vector<GlyphID>{glyph},
                    std::vector<float>{0.f}, std::vector<float>{0.f});
  TextBlob blob(std::move(runs));

  Paint gradient_paint;
  Point points[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  gradient_paint.SetShader(Shader::MakeLinear(points, colors, nullptr, 2));
  const Paint paints[] = {gradient_paint, MakeImagePaint(MakeTestImage())};
  for (auto paint : paints) {
    paint.SetAlphaF(0.5f);
    paint.SetColorFilter(ColorFilters::Blend(0xFF00FF00, BlendMode::kSrcATop));
    const auto shader = paint.GetShader();
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    auto* device = context.device();

    PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);
    EXPECT_EQ(device->texture_count(), 0u);
    EXPECT_EQ(device->sampler_count(), 0u);
    EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("SolidVertexColor"));
    EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Gradient"));
    EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Texture"));

    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawEmojiText, paint, false,
        [&](Canvas* canvas) { canvas->DrawTextBlob(&blob, 4.f, 24.f, paint); });
    EXPECT_EQ(device->last_instance_counts(), std::vector<uint32_t>{1u});
    EXPECT_EQ(paint.GetShader(), shader);
  }
}

TEST(PrecompileDrawTest, TextPrecompilesDefaultBlendPlan) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  auto precompile_context = MakePrecompileContext(context, false);

  Paint paint;
  paint.SetBlendMode(BlendMode::kOverlay);
  const auto pipeline_count = device->render_pipeline_count();
  precompile_context->PrecompileDraw(PrecompileDrawType::kDrawText, paint);

  // Precompile only the normal texture-copy plan for this device.
  EXPECT_EQ(device->render_pipeline_count() - pipeline_count, 1u);
}

TEST(PrecompileDrawTest, SupportsGradientTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, nullptr, 2));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsTextureTextDrawWithoutGPUResources) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  Paint paint = MakeImagePaint(MakeTestImage());

  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_TRUE(device->HasFragmentFunctionLabel("FS_Texture_TextA8"));
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsSDFTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawSDFText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsEmojiTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
  EXPECT_TRUE(
      device->HasFragmentFunctionLabel("FS_SolidVertexColor_TextColor"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel(
      "FS_SolidVertexColor_TextColorSwizzleRB"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Emoji"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, EmojiPrecompilesDefaultBlendPlan) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetBlendMode(BlendMode::kOverlay);
  const auto pipeline_count = device->render_pipeline_count();
  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  // RGBA and BGRA each precompile the normal texture-copy plan.
  EXPECT_EQ(device->render_pipeline_count() - pipeline_count, 2u);
}

TEST(PrecompileDrawTest, SupportsGradientEmojiTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, nullptr, 2));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
  EXPECT_TRUE(
      device->HasFragmentFunctionLabel("FS_SolidVertexColor_TextColor"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel(
      "FS_SolidVertexColor_TextColorSwizzleRB"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Emoji"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

}  // namespace skity
