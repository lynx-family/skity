// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <skity/effect/shader.hpp>
#include <skity/graphic/image.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>
#include <skity/render/precompile_context.hpp>
#include <skity/text/text_blob.hpp>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"
#include "src/render/hw/hw_static_buffer.hpp"
#include "src/render/text/glyph_run.hpp"
#include "test/ut/gpu/fake_gpu_context.hpp"
#include "test/ut/text/recording_glyph_draw_target.hpp"

namespace skity::test {

TEST(HWTextDrawTest, GeometryBorrowsResources) {
  static_assert(
      std::is_constructible_v<WGSLTextGeometry, const TextGeometryData&>);
  static_assert(!std::is_constructible_v<WGSLTextGeometry, TextGeometryData&&>);
  static_assert(
      !std::is_constructible_v<WGSLTextGeometry, const TextGeometryData&&>);

  auto texture = std::make_shared<FakeGPUTexture>(GPUTextureDescriptor{});
  auto sampler = std::make_shared<FakeSampler>(GPUSamplerDescriptor{});
  TextGeometryData data;
  data.textures[0] = texture;
  data.sampler = sampler;
  WGSLTextGeometry geometry(data);

  // Geometry borrows the resource holders without copying or moving them.
  EXPECT_EQ(data.textures[0], texture);
  EXPECT_EQ(data.sampler, sampler);
  EXPECT_EQ(texture.use_count(), 2);
  EXPECT_EQ(sampler.use_count(), 2);
}

TEST(HWTextDrawTest, FillAndStrokeMergePreservingInstanceColors) {
  auto typeface = Typeface::MakeFromFile(SKITY_FONT_DIR
                                         "fonts/resources/Roboto-Regular.ttf");
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph = typeface->UnicharToGlyph('O');
  ASSERT_NE(glyph, 0);
  const float position = 0.f;
  for (auto style :
       {Paint::kStrokeAndFill_Style, Paint::kStrokeThenFill_Style}) {
    SCOPED_TRACE(static_cast<int>(style));
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    ArenaAllocator arena;
    Paint paint;
    paint.SetStyle(style);
    paint.SetStrokeWidth(2.f);
    paint.SetFillColor(1.f, 0.f, 0.f, 0.5f);
    paint.SetStrokeColor(0.f, 0.f, 1.f, 0.75f);
    const Font font(typeface, 20.f);
    auto runs = GlyphRunBuilder::Build(
        {1, &glyph, Point{0.f, 0.f, 0.f, 1.f}, &position, &position, font},
        paint, {*context.GetAtlasManager(), arena, 1.f, Matrix{}});
    ASSERT_EQ(runs.size(), 2u);
    auto* first =
        RecordAtlasGlyphRun(*runs[0], {arena, Matrix{}, Matrix{}, false}).draw;
    auto* second =
        RecordAtlasGlyphRun(*runs[1], {arena, Matrix{}, Matrix{}, false}).draw;
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    first->SetScissorBox(Rect::MakeWH(32.f, 32.f));
    second->SetScissorBox(Rect::MakeWH(32.f, 32.f));
    auto plan = ResolveHWBlendPlan(BlendMode::kSrcOver, true, false,
                                   context.device()->GetCaps(), true);
    ASSERT_TRUE(plan.has_value());
    first->SetBlendPlan(*plan);
    second->SetBlendPlan(*plan);
    ASSERT_TRUE(first->MergeIfPossible(second));

    HWStageBuffer stage_buffer(context.device());
    HWStaticBuffer static_buffer(context.device());
    HWDrawContext draw_context{};
    draw_context.arena_allocator = &arena;
    draw_context.gpuContext = &context;
    draw_context.pipelineLib = context.GetPipelineLib();
    draw_context.stageBuffer = &stage_buffer;
    draw_context.static_buffer = &static_buffer;
    first->SetClipDepth(0);
    auto state = first->Prepare(&draw_context);
    first->GenerateCommand(&draw_context, state);
    std::vector<uint32_t> instance_counts;
    FakeRenderPass pass(GPURenderPassDescriptor{}, instance_counts);
    first->Draw(&pass, nullptr);
    ASSERT_EQ(pass.GetCommands().size(), 1u);
    const auto* command = pass.GetCommands().front();
    ASSERT_EQ(command->instance_count, 2u);
    EXPECT_EQ(command->pipeline->GetDescriptor().fragment_function->GetLabel(),
              "FS_SolidVertexColor_TextA8");
    const auto& layout = command->pipeline->GetDescriptor().buffers->at(1);
    // The last instance attribute is color; merging must preserve both the
    // original draw order and each run's independently selected paint color.
    for (size_t i = 0; i < 2; ++i) {
      const auto* color = stage_buffer.ToInstance<Color4f>(
          command->instance_buffer.offset + i * layout.array_stride +
          layout.attributes.back().offset);
      const bool stroke =
          style == Paint::kStrokeThenFill_Style ? i == 0 : i == 1;
      EXPECT_EQ(*color, stroke ? paint.GetStrokeColor() : paint.GetFillColor());
    }
  }
}

TEST(HWTextDrawTest, BlendRoutesKeepRunsWholeAndProtectDrawBoundaries) {
  struct TestCase {
    bool texture_copy;
    bool native_blend;
    bool coherent;
  };
  const TestCase cases[] = {{true, false, false},
                            {true, true, false},
                            {true, true, true},
                            {false, false, false}};
  struct DrawCase {
    bool second_draw;
    float offset_x;
  };
  const DrawCase draw_cases[] = {{false, 0.f}, {true, 0.f}, {true, 64.f}};
  for (bool emoji : {false, true}) {
    auto typeface = Typeface::MakeFromFile(
        emoji ? SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf"
              : SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(typeface, nullptr);
    const GlyphID glyph = typeface->UnicharToGlyph(emoji ? 0x1F600 : 'O');
    ASSERT_NE(glyph, 0);
    std::vector<TextRun> runs;
    // Internal overlap is intentionally not split. FakeGPU only checks draw
    // structure and pipeline selection, not pixels on restricted blend routes.
    runs.emplace_back(Font(typeface, 20.f), std::vector<GlyphID>{glyph, glyph},
                      std::vector<float>{0.f, 0.f},
                      std::vector<float>{0.f, 0.f});
    TextBlob blob(std::move(runs));
    for (const auto& test : cases) {
      for (const auto& draw_case : draw_cases) {
        SCOPED_TRACE(::testing::Message()
                     << "emoji=" << emoji << " copy=" << test.texture_copy
                     << " native=" << test.native_blend
                     << " coherent=" << test.coherent
                     << " second_draw=" << draw_case.second_draw
                     << " offset=" << draw_case.offset_x);
        FakeGPUContext context;
        ASSERT_TRUE(context.Init());
        context.SetEnableMergingDrawCall(true);
        auto* device = context.device();
        auto caps = std::make_unique<GPUCaps>();
        caps->supports_native_advanced_blend = test.native_blend;
        caps->supports_native_advanced_blend_coherent = test.coherent;
        caps->native_blend_shader_variant = test.native_blend;
        device->InitCaps(std::move(caps));

        Paint paint;
        paint.SetBlendMode(BlendMode::kOverlay);
        paint.SetAlphaF(0.5f);
        context.CreatePrecompileContext(PrecompileColorType::kRGBA, false)
            ->PrecompileDraw(emoji ? PrecompileDrawType::kDrawEmojiText
                                   : PrecompileDrawType::kDrawText,
                             paint);
        const auto pipeline_count = device->render_pipeline_count();
        // Layer-specific no-copy fallbacks are created on demand, not
        // prewarmed.
        device->set_disallow_shader_pipeline_creation(test.texture_copy);
        GPUSurfaceDescriptor desc{};
        desc.width = 160;
        desc.height = 64;
        desc.sample_count = 1;
        auto surface = context.CreateSurface(&desc);
        static_cast<FakeGPUSurface*>(surface.get())
            ->SetSupportsTextureCopyDstRead(test.texture_copy);
        auto* canvas = surface->LockCanvas(true);
        canvas->DrawTextBlob(&blob, 8.f, 32.f, paint);
        if (draw_case.second_draw) {
          canvas->DrawTextBlob(&blob, 8.f + draw_case.offset_x, 32.f, paint);
        }
        canvas->Flush();
        surface->Flush();

        const bool needs_copy =
            test.texture_copy && !(emoji && test.native_blend);
        const bool non_coherent_native =
            emoji && test.native_blend && !test.coherent;
        const bool separate =
            needs_copy || (non_coherent_native && draw_case.offset_x == 0.f);
        EXPECT_EQ(device->last_instance_counts(),
                  !draw_case.second_draw ? (std::vector<uint32_t>{2u})
                  : separate             ? (std::vector<uint32_t>{2u, 2u})
                                         : (std::vector<uint32_t>{4u}));
        EXPECT_EQ(device->last_render_pass_count(),
                  needs_copy ? (draw_case.second_draw ? 3u : 2u) : 1u);
        EXPECT_EQ(device->render_pipeline_count() - pipeline_count,
                  test.texture_copy ? 0u : 1u);
        EXPECT_EQ(device->disallowed_shader_function_count(), 0u);
        EXPECT_EQ(device->disallowed_render_pipeline_count(), 0u);
        EXPECT_EQ(device->disallowed_clone_pipeline_count(), 0u);
      }
    }
  }
}

TEST(HWTextDrawTest, SnapshotsShaderLocalMatrix) {
  const Matrix matrices[] = {
      Matrix::Translate(3.f, 5.f) * Matrix::Scale(2.f, 4.f),
      Matrix::Translate(7.f, 11.f) * Matrix::Scale(4.f, 2.f)};
  for (bool image_shader : {false, true}) {
    for (auto effect :
         {TextAtlasEffect::kA8Coverage, TextAtlasEffect::kSDFCoverage}) {
      SCOPED_TRACE(::testing::Message()
                   << "image=" << image_shader
                   << " effect=" << static_cast<int>(effect));
      FakeGPUContext context;
      ASSERT_TRUE(context.Init());
      ArenaAllocator arena;
      Paint paint;
      if (image_shader) {
        auto pixmap = std::make_shared<Pixmap>(
            1, 1, AlphaType::kPremul_AlphaType, ColorType::kRGBA);
        paint.SetShader(Shader::MakeShader(Image::MakeImage(std::move(pixmap)),
                                           SamplingOptions{}, TileMode::kDecal,
                                           TileMode::kDecal, Matrix{}));
      } else {
        Point points[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
        Vec4 colors[] = {Colors::kRed, Colors::kBlue};
        paint.SetShader(Shader::MakeLinear(points, colors, nullptr, 2));
      }
      TextGeometryData::BatchedTexture textures{};
      GPUTextureDescriptor texture_desc{};
      texture_desc.width = texture_desc.height = 32;
      textures[0] = context.device()->CreateTexture(texture_desc);
      auto sampler = context.device()->CreateSampler(GPUSamplerDescriptor{});
      auto plan = ResolveHWBlendPlan(BlendMode::kSrcOver, true, false,
                                     context.device()->GetCaps(), true);
      ASSERT_TRUE(plan.has_value());
      std::vector<HWDynamicTextDraw*> draws;
      for (const auto& matrix : matrices) {
        paint.GetShader()->SetLocalMatrix(matrix);
        ArrayList<GlyphRect, 16> glyph_rects;
        glyph_rects.SetArenaAllocator(&arena);
        glyph_rects.emplace_back(Vec4{0.f, 0.f, 16.f, 16.f}, Vec2{0.f, 0.f},
                                 Vec2{16.f, 16.f});
        auto* draw = arena.Make<HWDynamicTextDraw>(
            Matrix{}, Matrix{}, std::move(glyph_rects), paint, textures,
            sampler, effect, false, Matrix{});
        draw->SetScissorBox(Rect::MakeWH(32.f, 32.f));
        draw->SetClipDepth(0);
        draw->SetBlendPlan(*plan);
        draws.push_back(draw);
      }
      // Both draws are recorded before preparation, just as before Flush.
      paint.GetShader()->SetLocalMatrix(Matrix::Translate(99.f, 101.f));
      HWStageBuffer stage_buffer(context.device());
      HWStaticBuffer static_buffer(context.device());
      HWDrawContext draw_context{};
      draw_context.arena_allocator = &arena;
      draw_context.gpuContext = &context;
      draw_context.pipelineLib = context.GetPipelineLib();
      draw_context.stageBuffer = &stage_buffer;
      draw_context.static_buffer = &static_buffer;
      std::vector<uint32_t> instance_counts;
      FakeRenderPass pass(GPURenderPassDescriptor{}, instance_counts);
      for (auto* draw : draws) {
        auto state = draw->Prepare(&draw_context);
        draw->GenerateCommand(&draw_context, state);
        draw->Draw(&pass, nullptr);
      }
      ASSERT_EQ(pass.GetCommands().size(), 2u);
      // Matrix values must not introduce another shader/pipeline variant.
      EXPECT_EQ(pass.GetCommands()[0]->pipeline,
                pass.GetCommands()[1]->pipeline);
      for (size_t i = 0; i < draws.size(); ++i) {
        const auto* command = pass.GetCommands()[i];
        const UniformBinding* matrix_binding = nullptr;
        for (const auto& binding : command->uniform_bindings) {
          if (binding.group == 0 && binding.binding == 1) {
            matrix_binding = &binding;
            break;
          }
        }
        ASSERT_NE(matrix_binding, nullptr);
        size_t offset = matrix_binding->buffer.offset;
        if (image_shader) {
          const auto* entry =
              command->pipeline->GetBindingGroup(0)->GetEntry(1);
          auto* info =
              static_cast<wgx::StructDefinition*>(entry->type_definition.get());
          offset += info->GetMember("inv_matrix")->offset;
        }
        Matrix expected;
        ASSERT_TRUE(matrices[i].Invert(&expected));
        EXPECT_EQ(*stage_buffer.ToInstance<Matrix>(offset), expected);
      }
    }
  }
}

}  // namespace skity::test
