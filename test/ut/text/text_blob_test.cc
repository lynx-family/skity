// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <skity/text/font_manager.hpp>
#include <skity/text/text_blob.hpp>
#include <skity/text/text_run.hpp>
#include <string>

#include "src/text/scaler_context.hpp"

using namespace skity;

namespace {

// A mock Typeface implementation for testing purposes. It mirrors the one in
// text_run_test.cc: it never provides a real ScalerContext
// (OnCreateScalerContext returns nullptr), so it must only be used in cases
// that never query glyph metrics. Font::LoadGlyphMetrics (and anything that
// calls it, like TextBlob::GetBoundsRect/ComputeBounds) dereferences the
// ScalerContext returned here, so those cases use a real typeface from
// FontManager instead.
class MockTypeface : public Typeface {
 public:
  MockTypeface() : Typeface(FontStyle()) {}
  ~MockTypeface() override = default;

 protected:
  int OnGetTableTags(FontTableTag*) const override { return 0; }
  size_t OnGetTableData(FontTableTag, size_t, size_t, void*) const override {
    return 0;
  }
  void OnCharsToGlyphs(const uint32_t*, int, GlyphID*) const override {}
  std::shared_ptr<Data> OnGetData() override { return nullptr; }
  uint32_t OnGetUPEM() const override { return 2048; }
  bool OnContainsColorTable() const override { return false; }
  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc*) const override {
    return nullptr;
  }
  VariationPosition OnGetVariationDesignPosition() const override {
    return VariationPosition();
  }
  std::vector<VariationAxis> OnGetVariationDesignParameters() const override {
    return {};
  }
  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments&) const override {
    return nullptr;
  }

  void OnGetFontDescriptor(FontDescriptor&) const override {}
};

}  // namespace

class TextBlobTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_typeface = std::make_shared<MockTypeface>();
    mock_font = Font{mock_typeface, 20.f};

    // Real typeface used by cases that need actual glyph metrics
    // (GetBoundsRect / ComputeBounds). Guard with HasRealTypeface() +
    // GTEST_SKIP() in case it is unavailable in a given environment, mirroring
    // typeface_test.cc / scaler_context_cache_test.cc in this directory.
    default_typeface =
        FontManager::RefDefault()->GetDefaultTypeface(FontStyle());
  }

  bool HasRealTypeface() const { return default_typeface != nullptr; }

  std::shared_ptr<Typeface> mock_typeface;
  Font mock_font;
  std::shared_ptr<Typeface> default_typeface;
};

TEST_F(TextBlobTest, ConstructFromSingleTextRun) {
  std::vector<GlyphID> glyph_ids = {10, 11, 12};
  std::vector<float> pos_x = {0.f, 12.f, 25.f};
  std::vector<float> pos_y = {0.f, 1.f, -1.f};

  TextRun run{mock_font, glyph_ids, pos_x, pos_y};

  TextBlob blob(std::vector<TextRun>{run});

  ASSERT_EQ(blob.GetTextRun().size(), 1u);
  const TextRun& round_tripped = blob.GetTextRun()[0];

  EXPECT_EQ(round_tripped.GetGlyphInfo(), glyph_ids);
  EXPECT_EQ(round_tripped.GetPosX(), pos_x);
  EXPECT_EQ(round_tripped.GetPosY(), pos_y);
  EXPECT_FLOAT_EQ(round_tripped.GetFontSize(), 20.f);
  EXPECT_EQ(round_tripped.LockTypeface(), mock_typeface);
}

TEST_F(TextBlobTest, ConstructFromMultipleTextRuns) {
  std::vector<GlyphID> glyphs_a = {1, 2};
  std::vector<GlyphID> glyphs_b = {3, 4, 5};

  TextRun run_a{mock_font, glyphs_a};
  TextRun run_b{mock_font, glyphs_b};

  TextBlob blob(std::vector<TextRun>{run_a, run_b});

  ASSERT_EQ(blob.GetTextRun().size(), 2u);
  EXPECT_EQ(blob.GetTextRun()[0].GetGlyphInfo(), glyphs_a);
  EXPECT_EQ(blob.GetTextRun()[1].GetGlyphInfo(), glyphs_b);
}

TEST_F(TextBlobTest, ConstructFromEmptyRunList) {
  TextBlob blob(std::vector<TextRun>{});

  EXPECT_TRUE(blob.GetTextRun().empty());
}

TEST_F(TextBlobTest, GetBoundsRectOnEmptyBlobIsEmpty) {
  TextBlob blob(std::vector<TextRun>{});

  Rect rect = blob.GetBoundsRect();
  EXPECT_TRUE(rect.IsEmpty());
  EXPECT_EQ(rect, Rect::MakeEmpty());

  Vec2 size = blob.GetBoundSize();
  EXPECT_FLOAT_EQ(size.x, 0.f);
  EXPECT_FLOAT_EQ(size.y, 0.f);
}

TEST_F(TextBlobTest, GetBoundsRectSingleGlyphAtOrigin) {
  if (!HasRealTypeface()) {
    GTEST_SKIP();
  }

  Font font(default_typeface, 20.f);
  GlyphID glyph_id = default_typeface->UnicharToGlyph('A');
  ASSERT_NE(glyph_id, 0);

  std::vector<GlyphID> glyph_ids = {glyph_id};
  std::vector<float> pos_x = {0.f};
  std::vector<float> pos_y = {0.f};

  TextRun run{font, glyph_ids, pos_x, pos_y};
  TextBlob blob(std::vector<TextRun>{run});

  // Recreate exactly what TextBlob::GetBoundsRect() computes internally (see
  // src/text/text_blob.cc), so the expected values are not hard-coded
  // font-specific magic numbers.
  Paint metrics_paint;
  metrics_paint.SetTextSize(font.GetSize());
  const GlyphData* glyph_data[1] = {};
  font.LoadGlyphMetrics(&glyph_id, 1, glyph_data, metrics_paint);
  ASSERT_NE(glyph_data[0], nullptr);

  float expected_left = 0.f + glyph_data[0]->GetHoriBearingX();
  float expected_top = 0.f - glyph_data[0]->GetYMax();
  float expected_right = expected_left + glyph_data[0]->GetWidth();
  float expected_bottom = 0.f - glyph_data[0]->GetYMin();

  Rect rect = blob.GetBoundsRect();
  EXPECT_FLOAT_EQ(rect.Left(), expected_left);
  EXPECT_FLOAT_EQ(rect.Top(), expected_top);
  EXPECT_FLOAT_EQ(rect.Right(), expected_right);
  EXPECT_FLOAT_EQ(rect.Bottom(), expected_bottom);

  Vec2 size = blob.GetBoundSize();
  EXPECT_FLOAT_EQ(size.x, rect.Width());
  EXPECT_FLOAT_EQ(size.y, rect.Height());
}

TEST_F(TextBlobTest, GetBoundsRectAutoLayoutMultipleGlyphs) {
  if (!HasRealTypeface()) {
    GTEST_SKIP();
  }

  Font font(default_typeface, 18.f);
  GlyphID glyph_a = default_typeface->UnicharToGlyph('A');
  GlyphID glyph_b = default_typeface->UnicharToGlyph('B');
  ASSERT_NE(glyph_a, 0);
  ASSERT_NE(glyph_b, 0);

  std::vector<GlyphID> glyph_ids = {glyph_a, glyph_b};
  // No explicit positions: GetBoundsRect() lays glyphs out itself by
  // accumulating each glyph's advance, starting at x = 0.
  TextRun run{font, glyph_ids};
  ASSERT_TRUE(run.GetPosX().empty());
  ASSERT_TRUE(run.GetPosY().empty());

  TextBlob blob(std::vector<TextRun>{run});

  Rect rect = blob.GetBoundsRect();
  EXPECT_FALSE(rect.IsEmpty());
  EXPECT_GT(rect.Width(), 0.f);
  EXPECT_GE(rect.Height(), 0.f);

  Vec2 size = blob.GetBoundSize();
  EXPECT_FLOAT_EQ(size.x, rect.Width());
  EXPECT_FLOAT_EQ(size.y, rect.Height());
}

TEST_F(TextBlobTest, ComputeBoundsMatchesManualFormula) {
  if (!HasRealTypeface()) {
    GTEST_SKIP();
  }

  Font font(default_typeface, 24.f);
  GlyphID glyph_h = default_typeface->UnicharToGlyph('H');
  GlyphID glyph_i = default_typeface->UnicharToGlyph('i');
  ASSERT_NE(glyph_h, 0);
  ASSERT_NE(glyph_i, 0);

  std::vector<GlyphID> glyphs = {glyph_h, glyph_i};
  std::vector<float> pos_x = {0.f, 18.f};
  std::vector<float> pos_y = {0.f, 0.f};
  Paint paint;  // default kFill_Style -> no stroke contribution

  Rect rect = TextBlob::ComputeBounds(static_cast<uint32_t>(glyphs.size()),
                                      glyphs.data(), pos_x.data(), pos_y.data(),
                                      font, paint);

  // Recompute the same values using the formula documented in
  // src/text/text_blob.cc, instead of hard-coding font-specific numbers.
  std::vector<const GlyphData*> glyph_data(glyphs.size());
  font.LoadGlyphMetrics(glyphs.data(), static_cast<uint32_t>(glyphs.size()),
                        glyph_data.data(), paint);
  ASSERT_NE(glyph_data[0], nullptr);
  ASSERT_NE(glyph_data[1], nullptr);

  float expected_left = pos_x[0];
  float expected_right =
      pos_x[1] + glyph_data[1]->GetHoriBearingX() + glyph_data[1]->GetWidth();
  float expected_top = std::min(pos_y[0] - glyph_data[0]->GetHoriBearingY(),
                                pos_y[1] - glyph_data[1]->GetHoriBearingY());
  float expected_bottom = std::max(pos_y[0] - glyph_data[0]->GetYMin(),
                                   pos_y[1] - glyph_data[1]->GetYMin());

  EXPECT_FLOAT_EQ(rect.Left(), std::floor(expected_left));
  EXPECT_FLOAT_EQ(rect.Top(), std::floor(expected_top));
  EXPECT_FLOAT_EQ(rect.Right(), std::floor(expected_right));
  EXPECT_FLOAT_EQ(rect.Bottom(), std::floor(expected_bottom));
}

TEST_F(TextBlobTest, ComputeBoundsExpandsWithStrokeWidth) {
  if (!HasRealTypeface()) {
    GTEST_SKIP();
  }

  Font font(default_typeface, 24.f);
  GlyphID glyph_id = default_typeface->UnicharToGlyph('O');
  ASSERT_NE(glyph_id, 0);

  std::vector<GlyphID> glyphs = {glyph_id};
  std::vector<float> pos_x = {5.f};
  std::vector<float> pos_y = {5.f};

  Paint fill_paint;  // kFill_Style by default, stroke_width does not apply
  Rect fill_rect = TextBlob::ComputeBounds(1, glyphs.data(), pos_x.data(),
                                           pos_y.data(), font, fill_paint);

  Paint stroke_paint;
  stroke_paint.SetStyle(Paint::kStroke_Style);
  stroke_paint.SetStrokeWidth(4.f);
  Rect stroke_rect = TextBlob::ComputeBounds(1, glyphs.data(), pos_x.data(),
                                             pos_y.data(), font, stroke_paint);

  // ComputeBounds outsets the fill bounds by the paint's stroke width when
  // the style requests stroking, so the stroked bounds must be at least as
  // large as the filled ones.
  EXPECT_LE(stroke_rect.Left(), fill_rect.Left());
  EXPECT_LE(stroke_rect.Top(), fill_rect.Top());
  EXPECT_GE(stroke_rect.Right(), fill_rect.Right());
  EXPECT_GE(stroke_rect.Bottom(), fill_rect.Bottom());
}

TEST_F(TextBlobTest, BuildTextBlobReturnsNullWithoutTypeface) {
  TextBlobBuilder builder;
  Paint paint;  // no typeface set

  auto blob = builder.BuildTextBlob("A", paint);

  EXPECT_EQ(blob, nullptr);
}

TEST_F(TextBlobTest, BuildTextBlobWithMockTypefaceProducesExpectedGlyphCount) {
  TextBlobBuilder builder;
  Paint paint;
  paint.SetTypeface(mock_typeface);
  paint.SetTextSize(16.f);

  auto blob = builder.BuildTextBlob("Hi", paint);

  ASSERT_NE(blob, nullptr);
  ASSERT_EQ(blob->GetTextRun().size(), 1u);
  // "Hi" is two ASCII code points, so the single run should hold two glyphs.
  EXPECT_EQ(blob->GetTextRun()[0].GetGlyphInfo().size(), 2u);
}

TEST_F(TextBlobTest, BuildTextBlobStringOverloadMatchesCharOverload) {
  TextBlobBuilder builder;
  Paint paint;
  paint.SetTypeface(mock_typeface);
  paint.SetTextSize(16.f);

  auto blob_from_cstr = builder.BuildTextBlob("Hi", paint);
  auto blob_from_string = builder.BuildTextBlob(std::string("Hi"), paint);

  ASSERT_NE(blob_from_cstr, nullptr);
  ASSERT_NE(blob_from_string, nullptr);
  ASSERT_EQ(blob_from_cstr->GetTextRun().size(), 1u);
  ASSERT_EQ(blob_from_string->GetTextRun().size(), 1u);
  EXPECT_EQ(blob_from_cstr->GetTextRun()[0].GetGlyphInfo(),
            blob_from_string->GetTextRun()[0].GetGlyphInfo());
}

TEST_F(TextBlobTest, BuildTextBlobWithRealTypefaceEndToEnd) {
  if (!HasRealTypeface()) {
    GTEST_SKIP();
  }

  TextBlobBuilder builder;
  Paint paint;
  paint.SetTypeface(default_typeface);
  paint.SetTextSize(20.f);

  auto blob = builder.BuildTextBlob("Hi", paint);

  ASSERT_NE(blob, nullptr);
  ASSERT_FALSE(blob->GetTextRun().empty());
  EXPECT_FALSE(blob->GetTextRun()[0].GetGlyphInfo().empty());

  Rect rect = blob->GetBoundsRect();
  EXPECT_FALSE(rect.IsEmpty());
}
