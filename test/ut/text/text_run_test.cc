// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/text/text_run.hpp>

#include "src/text/scaler_context.hpp"

namespace skity {

// A mock Typeface implementation for testing purposes.
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

  void OnGetFontDescriptor(FontDescriptor& desc) const override {}
};

class TextRunTest : public ::testing::Test {
 protected:
  void SetUp() override {
    typeface = std::make_shared<MockTypeface>();
    font = Font{typeface, 20.f};
    glyphs = {1, 2, 3, 4};
    pos_x = {0.f, 10.f, 22.f, 35.f};
    pos_y = {5.f, 5.f, 4.f, 6.f};
  }

  std::shared_ptr<Typeface> typeface;
  Font font;
  std::vector<GlyphID> glyphs;
  std::vector<float> pos_x;
  std::vector<float> pos_y;
};

TEST_F(TextRunTest, SimpleConstructor) {
  TextRun text_run{font, glyphs};

  EXPECT_EQ(text_run.GetFont().GetSize(), 20.f);
  EXPECT_EQ(text_run.GetGlyphInfo().size(), 4);
  EXPECT_EQ(text_run.GetGlyphInfo()[2], 3);
  EXPECT_TRUE(text_run.GetPosX().empty());
  EXPECT_TRUE(text_run.GetPosY().empty());
}

TEST_F(TextRunTest, ConstructorWithXPos) {
  TextRun text_run{font, glyphs, pos_x};

  EXPECT_EQ(text_run.GetPosX().size(), 4);
  EXPECT_FLOAT_EQ(text_run.GetPosX()[2], 22.f);
  EXPECT_TRUE(text_run.GetPosY().empty());
}

TEST_F(TextRunTest, ConstructorWithXYPos) {
  TextRun text_run{font, glyphs, pos_x, pos_y};

  EXPECT_EQ(text_run.GetPosX().size(), 4);
  EXPECT_EQ(text_run.GetPosY().size(), 4);
  EXPECT_FLOAT_EQ(text_run.GetPosX()[3], 35.f);
  EXPECT_FLOAT_EQ(text_run.GetPosY()[3], 6.f);
}

TEST_F(TextRunTest, Getters) {
  TextRun text_run{font, glyphs, pos_x, pos_y};

  EXPECT_EQ(text_run.GetFontSize(), 20.f);
  EXPECT_EQ(text_run.LockTypeface(), typeface);

  EXPECT_EQ(text_run.GetGlyphInfo().size(), glyphs.size());
  EXPECT_EQ(text_run.GetGlyphInfo()[0], glyphs[0]);

  EXPECT_EQ(text_run.GetPosX().size(), pos_x.size());
  EXPECT_FLOAT_EQ(text_run.GetPosX()[0], pos_x[0]);

  EXPECT_EQ(text_run.GetPosY().size(), pos_y.size());
  EXPECT_FLOAT_EQ(text_run.GetPosY()[0], pos_y[0]);
}

}  // namespace skity