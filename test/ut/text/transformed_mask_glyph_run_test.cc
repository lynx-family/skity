// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/text/transformed_mask_glyph_run.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "src/render/hw/draw/fragment/wgsl_text_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_text_geometry.hpp"
#include "src/render/hw/draw/hw_dynamic_text_draw.hpp"

namespace skity {
namespace transformed_mask {
namespace {

void SetDefaultBlendPlan(HWDraw* first, HWDraw* second) {
  auto plan = ResolveFixedFunctionBlendPlan(BlendMode::kSrcOver,
                                            /*has_fragment_mask=*/false,
                                            /*source_is_opaque=*/false)
                  .value();
  first->SetBlendPlan(plan);
  second->SetBlendPlan(plan);
}

}  // namespace

TEST(TransformedMaskGlyphRunTest, BitmapOnlyStrokeFallsBackToStrokeColorFill) {
  Paint paint;
  paint.SetStyle(Paint::kStroke_Style);
  paint.SetFillColor(Color_RED);
  paint.SetStrokeColor(Color_BLUE);

  Paint fallback = MakeBitmapOnlyFillPaint(paint);

  EXPECT_EQ(fallback.GetStyle(), Paint::kFill_Style);
  EXPECT_EQ(fallback.GetFillColor(), paint.GetStrokeColor());
}

TEST(TransformedMaskGlyphRunTest,
     BitmapOnlyFillAndCombinedStylesFallBackToFillColor) {
  Paint paint;
  paint.SetFillColor(Color_RED);
  paint.SetStrokeColor(Color_BLUE);

  for (Paint::Style style : {Paint::kFill_Style, Paint::kStrokeAndFill_Style,
                             Paint::kStrokeThenFill_Style}) {
    paint.SetStyle(style);
    Paint fallback = MakeBitmapOnlyFillPaint(paint);

    EXPECT_EQ(fallback.GetStyle(), Paint::kFill_Style);
    EXPECT_EQ(fallback.GetFillColor(), paint.GetFillColor());
  }
}

TEST(TransformedMaskGlyphRunTest, QuantizesCreationScaleDown) {
  constexpr float kBucketsPerOctave = 4.f;
  float input = 3.9f;
  float quantized = QuantizeCreationScaleDown(input);

  EXPECT_GT(quantized, 0.f);
  EXPECT_LE(quantized, input);
  EXPECT_GT(quantized * std::exp2(1.f / kBucketsPerOctave), input);
  EXPECT_FLOAT_EQ(QuantizeCreationScaleDown(1.f), 1.f);
  EXPECT_FLOAT_EQ(QuantizeCreationScaleDown(0.f), 0.f);
  EXPECT_FLOAT_EQ(
      QuantizeCreationScaleDown(std::numeric_limits<float>::infinity()), 0.f);
}

TEST(TransformedMaskGlyphRunTest, AccountsForAtlasAllocatorPadding) {
  AtlasConfig a8_config{AtlasFormat::A8, false};
  AtlasConfig rgba_config{AtlasFormat::RGBA32, false};

  EXPECT_EQ(MaximumAtlasGlyphDimension(a8_config), 508u);
  EXPECT_EQ(MaximumAtlasGlyphDimension(rgba_config), 252u);
}

TEST(TransformedMaskGlyphRunTest, ReducesScaleToFitPhysicalAtlasPixels) {
  constexpr float kLogicalGlyphDimension = 64.f;
  constexpr float kContextScale = 2.f;
  constexpr uint32_t kMaximumDimension = 252;
  float creation_scale = FitCreationScaleToAtlas(
      8.f, kLogicalGlyphDimension, kContextScale, kMaximumDimension);

  ASSERT_GT(creation_scale, 0.f);
  EXPECT_LT(creation_scale, 8.f);
  EXPECT_LE(kLogicalGlyphDimension * kContextScale * creation_scale + 2.f,
            static_cast<float>(kMaximumDimension));
}

TEST(TransformedMaskGlyphRunTest,
     ViewDifferenceReplacesCreationMatrixWithPositionMatrix) {
  Matrix perspective;
  perspective.SetPersp1(0.002f);
  Matrix position_matrix = Matrix::Translate(120.f, 80.f) * perspective *
                           Matrix::RotateDeg(-35.f, Vec3{1.f, 0.f, 0.f});
  Matrix creation_matrix = Matrix::Scale(2.25f, 2.25f);
  Matrix view_difference;

  ASSERT_TRUE(ComputeViewDifference(position_matrix, creation_matrix,
                                    &view_difference));

  Vec4 local_point{23.f, -11.f, 0.f, 1.f};
  Vec4 through_creation = view_difference * (creation_matrix * local_point);
  Vec4 through_position = position_matrix * local_point;
  for (int index = 0; index < 4; ++index) {
    EXPECT_NEAR(through_creation[index], through_position[index], 0.0001f);
  }

  // Context scale belongs to the surface MVP. Applying it after both sides
  // preserves the same relation; it must not be folded into view_difference.
  Matrix context_scale = Matrix::Scale(2.f, 2.f);
  Vec4 device_from_creation = context_scale * through_creation;
  Vec4 device_from_position = context_scale * through_position;
  for (int index = 0; index < 4; ++index) {
    EXPECT_NEAR(device_from_creation[index], device_from_position[index],
                0.0001f);
  }
}

TEST(TransformedMaskGlyphRunTest, RejectsSingularCreationMatrix) {
  Matrix view_difference;
  EXPECT_FALSE(ComputeViewDifference(Matrix(), Matrix::Scale(0.f, 1.f),
                                     &view_difference));
}

TEST(TransformedMaskGlyphRunTest, MapsFiniteBounds) {
  Matrix transform = Matrix::Translate(8.f, -5.f) * Matrix::Scale(2.f, 3.f);
  Rect mapped = MapBounds(transform, Rect::MakeLTRB(1.f, 2.f, 4.f, 6.f));

  EXPECT_EQ(mapped, Rect::MakeLTRB(10.f, 1.f, 16.f, 13.f));
}

TEST(TransformedMaskGlyphRunTest, UsesConservativeBoundsAcrossHorizon) {
  Matrix perspective;
  perspective.SetPersp0(-0.1f);
  Rect mapped = MapBounds(perspective, Rect::MakeLTRB(0.f, 0.f, 20.f, 10.f));

  EXPECT_LE(mapped.Left(), -1E8F);
  EXPECT_LE(mapped.Top(), -1E8F);
  EXPECT_GE(mapped.Right(), 1E8F);
  EXPECT_GE(mapped.Bottom(), 1E8F);
}

TEST(TransformedMaskGlyphRunTest, MergesTextDrawsWithSamePerspectiveTransform) {
  Matrix perspective;
  perspective.SetPersp0(0.002f);
  Matrix transform = Matrix::Translate(120.f, 80.f) * perspective *
                     Matrix::RotateDeg(-35.f, Vec3{1.f, 0.f, 0.f});
  WGSLTextSolidColorGeometry first_geometry(Matrix{}, {}, Paint{});
  WGSLTextSolidColorGeometry second_geometry(Matrix{}, {}, Paint{});
  WGSLColorEmojiFragment first_fragment({}, nullptr, false, 1.f);
  WGSLColorEmojiFragment second_fragment({}, nullptr, false, 1.f);
  HWDynamicTextDraw first_draw(transform, &first_geometry, &first_fragment);
  HWDynamicTextDraw second_draw(transform, &second_geometry, &second_fragment);
  SetDefaultBlendPlan(&first_draw, &second_draw);
  first_draw.SetLayerSpaceBounds(Rect::MakeLTRB(0.f, 0.f, 10.f, 10.f));
  second_draw.SetLayerSpaceBounds(Rect::MakeLTRB(20.f, 20.f, 30.f, 30.f));

  EXPECT_TRUE(first_draw.MergeIfPossible(&second_draw));
  EXPECT_EQ(first_draw.GetLayerSpaceBounds(),
            Rect::MakeLTRB(0.f, 0.f, 30.f, 30.f));
}

TEST(TransformedMaskGlyphRunTest, RejectsTextDrawsWithDifferentTransforms) {
  Matrix first_transform;
  first_transform.SetPersp0(0.002f);
  Matrix second_transform = first_transform;
  second_transform.PostTranslate(1.f, 0.f);
  WGSLTextSolidColorGeometry first_geometry(Matrix{}, {}, Paint{});
  WGSLTextSolidColorGeometry second_geometry(Matrix{}, {}, Paint{});
  WGSLColorEmojiFragment first_fragment({}, nullptr, false, 1.f);
  WGSLColorEmojiFragment second_fragment({}, nullptr, false, 1.f);
  HWDynamicTextDraw first_draw(first_transform, &first_geometry,
                               &first_fragment);
  HWDynamicTextDraw second_draw(second_transform, &second_geometry,
                                &second_fragment);
  SetDefaultBlendPlan(&first_draw, &second_draw);

  EXPECT_FALSE(first_draw.MergeIfPossible(&second_draw));
}

TEST(TransformedMaskGlyphRunTest, KeepsIdentityTextDrawMerging) {
  WGSLTextSolidColorGeometry first_geometry(Matrix{}, {}, Paint{});
  WGSLTextSolidColorGeometry second_geometry(Matrix{}, {}, Paint{});
  WGSLColorTextFragment first_fragment({}, nullptr);
  WGSLColorTextFragment second_fragment({}, nullptr);
  HWDynamicTextDraw first_draw(Matrix{}, &first_geometry, &first_fragment);
  HWDynamicTextDraw second_draw(Matrix{}, &second_geometry, &second_fragment);
  SetDefaultBlendPlan(&first_draw, &second_draw);

  EXPECT_TRUE(first_draw.MergeIfPossible(&second_draw));
}

}  // namespace transformed_mask
}  // namespace skity
