// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <skity/text/font.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/typeface.hpp>
#include <vector>

#include "concurrent_runner.h"
#include "src/text/ports/scaler_context_freetype.hpp"
#include "src/text/scaler_context.hpp"
#include "src/text/scaler_context_desc.hpp"

using namespace skity;

constexpr int kThreadCount = 8;
constexpr int kIterations = 500;
constexpr const char* kRobotoRegular =
    SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf";

struct RasterizedGlyph {
  float origin_x = 0.f;
  float origin_y = 0.f;
  uint32_t width = 0;
  uint32_t height = 0;
  std::vector<uint8_t> pixels;
};

RasterizedGlyph RasterizeGlyph(
    std::shared_ptr<Typeface> typeface, GlyphID glyph_id, uint8_t x_phase,
    uint8_t y_phase, float context_scale = 1.f,
    const Matrix22& transform = Matrix22{},
    Font::FontHinting hinting = Font::FontHinting::kNormal) {
  Font font(typeface, 48.f);
  font.SetSubpixel(true);
  font.SetHinting(hinting);
  Paint paint;
  ScalerContextDesc desc =
      ScalerContextDesc::MakeTransformed(font, paint, context_scale, transform);
  auto context = typeface->CreateScalerContext(&desc);
  EXPECT_NE(context, nullptr);
  if (!context) {
    return {};
  }

  GlyphData glyph(glyph_id);
  context->MakeGlyph(&glyph);
  const StrokeDesc stroke_desc{false, paint.GetStrokeWidth(),
                               paint.GetStrokeCap(), paint.GetStrokeJoin(),
                               paint.GetStrokeMiter()};
  context->GetImage(PackedGlyphID(glyph_id, x_phase, y_phase), &glyph,
                    stroke_desc);

  const GlyphBitmapData& image = glyph.Image();
  RasterizedGlyph result{image.origin_x,
                         image.origin_y,
                         static_cast<uint32_t>(image.width),
                         static_cast<uint32_t>(image.height),
                         {}};
  EXPECT_EQ(image.format, BitmapFormat::kGray8);
  EXPECT_NE(image.buffer, nullptr);

  const size_t tight_row_bytes = result.width;
  EXPECT_GE(image.RowBytes(), tight_row_bytes);
  if (image.buffer && image.RowBytes() >= tight_row_bytes) {
    result.pixels.resize(tight_row_bytes * result.height);
    for (size_t y = 0; y < result.height; ++y) {
      std::memcpy(result.pixels.data() + y * tight_row_bytes,
                  image.buffer + y * image.RowBytes(), tight_row_bytes);
    }
  }
  if (image.need_free) {
    std::free(image.buffer);
  }
  return result;
}

class TypefaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto font_manager = FontManager::RefDefault();

    default_typeface = font_manager->GetDefaultTypeface(FontStyle());

    constexpr FontStyle::Weight kAllWeights[] = {
        FontStyle::Weight::kInvisible_Weight,
        FontStyle::Weight::kThin_Weight,
        FontStyle::Weight::kExtraLight_Weight,
        FontStyle::Weight::kLight_Weight,
        FontStyle::Weight::kNormal_Weight,
        FontStyle::Weight::kMedium_Weight,
        FontStyle::Weight::kSemiBold_Weight,
        FontStyle::Weight::kBold_Weight,
        FontStyle::Weight::kExtraBold_Weight,
        FontStyle::Weight::kBlack_Weight,
        FontStyle::Weight::kExtraBlack_Weight,
    };

    for (FontStyle::Weight w : kAllWeights) {
      FontStyle weight_style{w, FontStyle::Width::kNormal_Width,
                             FontStyle::Slant::kUpright_Slant};
      default_weighted_typefaces.push_back(
          font_manager->GetDefaultTypeface(weight_style));
    }

    constexpr FontStyle::Slant kAllSlant[] = {FontStyle::Slant::kUpright_Slant,
                                              FontStyle::Slant::kItalic_Slant,
                                              FontStyle::Slant::kOblique_Slant};
    for (FontStyle::Slant s : kAllSlant) {
      FontStyle weight_style{FontStyle::Weight::kNormal_Weight,
                             FontStyle::Width::kNormal_Width, s};
      default_slanted_typefaces.push_back(
          font_manager->GetDefaultTypeface(weight_style));
    }
  }

  std::shared_ptr<skity::Typeface> default_typeface;
  std::vector<std::shared_ptr<skity::Typeface>> default_weighted_typefaces;
  std::vector<std::shared_ptr<skity::Typeface>> default_slanted_typefaces;
};

TEST_F(TypefaceTest, DefaultTypefaceIsValid) {
  ASSERT_NE(default_typeface, nullptr);
  for (auto typeface : default_weighted_typefaces) {
    ASSERT_NE(typeface, nullptr);
  }
}

TEST_F(TypefaceTest, FontStyleFlagsAreConsistent) {
  FontStyle style = default_typeface->GetFontStyle();

  EXPECT_FALSE(default_typeface->IsBold());
  EXPECT_FALSE(default_typeface->IsItalic());

  EXPECT_EQ(style.weight(), FontStyle::Weight::kNormal_Weight);
  EXPECT_EQ(style.width(), FontStyle::Width::kNormal_Width);
  EXPECT_EQ(style.slant(), FontStyle::Slant::kUpright_Slant);

  for (auto typeface : default_weighted_typefaces) {
    FontStyle style = typeface->GetFontStyle();
    if (style.weight() >= FontStyle::kSemiBold_Weight) {
      EXPECT_TRUE(typeface->IsBold());
    } else {
      EXPECT_FALSE(typeface->IsBold());
    }
  }

  for (auto typeface : default_slanted_typefaces) {
    FontStyle style = typeface->GetFontStyle();
    if (style.slant() != FontStyle::kUpright_Slant) {
      EXPECT_TRUE(typeface->IsItalic());
    } else {
      EXPECT_FALSE(typeface->IsItalic());
    }
  }
}

TEST_F(TypefaceTest, TypefaceIdIsStable) {
  EXPECT_EQ(default_typeface->TypefaceId(), default_typeface->TypefaceId());

  auto cjk_typeface1 = FontManager::RefDefault()->MatchFamilyStyleCharacter(
      nullptr, FontStyle(), nullptr, 0, 23383);
  auto cjk_typeface2 = FontManager::RefDefault()->MatchFamilyStyleCharacter(
      nullptr, FontStyle(), nullptr, 0, 33410);
  EXPECT_EQ(cjk_typeface1->TypefaceId(), cjk_typeface2->TypefaceId());
}

TEST_F(TypefaceTest, MatchFamilyStyleCharacterFallsBackWhenBcp47Misses) {
  const char* bcp47[] = {"zz-Zzzz"};
  auto typeface = FontManager::RefDefault()->MatchFamilyStyleCharacter(
      nullptr, FontStyle(), bcp47, 1, 23383);

  ASSERT_NE(typeface, nullptr);
  EXPECT_NE(typeface->UnicharToGlyph(23383), 0);
}

TEST_F(TypefaceTest, MatchFamilyStyleCharacterFindsEmojiBcp47) {
  const char* bcp47[] = {"und-Zsye"};
  auto typeface = FontManager::RefDefault()->MatchFamilyStyleCharacter(
      nullptr, FontStyle(), bcp47, 1, 0x1F60A);

  ASSERT_NE(typeface, nullptr);
  EXPECT_NE(typeface->UnicharToGlyph(0x1F60A), 0);
}

TEST_F(TypefaceTest, UnicharToGlyphBasic) {
  GlyphID g1 = default_typeface->UnicharToGlyph('A');
  GlyphID g2 = default_typeface->UnicharToGlyph('B');

  EXPECT_NE(g1, 0);
  EXPECT_NE(g2, 0);
}

TEST_F(TypefaceTest, UnicharsToGlyphsBatch) {
  uint32_t chars[] = {'A', 'B', 'C'};
  GlyphID glyphs[3] = {};

  default_typeface->UnicharsToGlyphs(chars, 3, glyphs);

  for (GlyphID g : glyphs) {
    EXPECT_NE(g, 0);
  }
}

TEST_F(TypefaceTest, ContainGlyphBasic) {
  EXPECT_TRUE(default_typeface->ContainGlyph('A'));
  // illegal unicode
  EXPECT_FALSE(default_typeface->ContainGlyph(0x10FFFF + 1));
}

/**
 * Verifies that UnicharToGlyph can be safely called concurrently.
 * This test stresses internal glyph cache access under contention.
 */
TEST_F(TypefaceTest, UnicharToGlyphThreadSafe) {
  ConcurrentRunner runner(kThreadCount, kIterations);

  runner.Run([&](int i) {
    uint32_t c = 'A';
    GlyphID g = default_typeface->UnicharToGlyph(c);
    EXPECT_NE(g, 0);
  });
}

/**
 * Verifies that UnicharsToGlyphs is thread-safe under concurrent execution.
 */
TEST_F(TypefaceTest, UnicharsToGlyphsThreadSafe) {
  ConcurrentRunner runner(kThreadCount, kIterations);

  runner.Run([&](int) {
    uint32_t chars[] = {'A', 'B', 'C'};
    GlyphID glyphs[3] = {};
    default_typeface->UnicharsToGlyphs(chars, 3, glyphs);

    for (GlyphID g : glyphs) {
      EXPECT_NE(g, 0);
    }
  });
}

TEST_F(TypefaceTest, TableCountAndTagsConsistent) {
  int count = default_typeface->CountTables();
  EXPECT_GE(count, 0);

  std::vector<FontTableTag> tags(count);
  int copied = default_typeface->GetTableTags(tags.data());

  EXPECT_EQ(copied, count);
}

TEST_F(TypefaceTest, TableSizeAndDataConsistent) {
  int count = default_typeface->CountTables();
  if (count == 0) return;

  std::vector<FontTableTag> tags(count);
  default_typeface->GetTableTags(tags.data());

  FontTableTag tag = tags[0];
  size_t size = default_typeface->GetTableSize(tag);

  EXPECT_GT(size, 0u);

  std::vector<uint8_t> buffer(size);
  size_t copied = default_typeface->GetTableData(tag, 0, size, buffer.data());

  EXPECT_EQ(copied, size);
}

/**
 * Verifies table-related APIs are thread-safe under concurrent access.
 */
TEST_F(TypefaceTest, FontTableApisThreadSafe) {
  ConcurrentRunner runner(kThreadCount, kIterations);

  runner.Run([&](int) {
    int count = default_typeface->CountTables();
    if (count <= 0) return;

    std::vector<FontTableTag> tags(count);
    default_typeface->GetTableTags(tags.data());

    FontTableTag tag = tags[0];
    size_t size = default_typeface->GetTableSize(tag);
    if (size == 0) return;

    std::vector<uint8_t> buffer(size);
    default_typeface->GetTableData(tag, 0, size, buffer.data());
  });
}

TEST_F(TypefaceTest, GetDataNotNull) {
  auto data = default_typeface->GetData();
  ASSERT_NE(data, nullptr);
  EXPECT_GT(data->Size(), 0u);
}

TEST_F(TypefaceTest, UnitsPerEmIsValid) {
  uint32_t upem = default_typeface->GetUnitsPerEm();
  EXPECT_GT(upem, 0u);
}

TEST_F(TypefaceTest, CreateScalerContextBasic) {
  ScalerContextDesc desc{};
  auto ctx = default_typeface->CreateScalerContext(&desc);
  EXPECT_NE(ctx, nullptr);
}

/**
 * Verifies that CreateScalerContext can be called concurrently without crashes.
 */
TEST_F(TypefaceTest, CreateScalerContextThreadSafe) {
  ConcurrentRunner runner(kThreadCount, kIterations);

  runner.Run([&](int) {
    ScalerContextDesc desc{};
    auto ctx = default_typeface->CreateScalerContext(&desc);
    EXPECT_NE(ctx, nullptr);
  });
}

TEST(FreeTypeScalerContextTest, RasterizesPackedSubpixelPhases) {
  auto typeface = Typeface::MakeFromFile(kRobotoRegular);
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph_id = typeface->UnicharToGlyph('H');
  ASSERT_NE(glyph_id, 0);

  const RasterizedGlyph phase_0 = RasterizeGlyph(typeface, glyph_id, 0, 0);
  const RasterizedGlyph x_phase_1 = RasterizeGlyph(typeface, glyph_id, 1, 0);
  const RasterizedGlyph y_phase_1 = RasterizeGlyph(typeface, glyph_id, 0, 1);

  ASSERT_FALSE(phase_0.pixels.empty());
  ASSERT_FALSE(x_phase_1.pixels.empty());
  ASSERT_FALSE(y_phase_1.pixels.empty());
  EXPECT_TRUE(phase_0.width != x_phase_1.width ||
              phase_0.height != x_phase_1.height ||
              phase_0.pixels != x_phase_1.pixels);
  EXPECT_TRUE(phase_0.width != y_phase_1.width ||
              phase_0.height != y_phase_1.height ||
              phase_0.pixels != y_phase_1.pixels);

  EXPECT_NEAR(phase_0.origin_x, std::round(phase_0.origin_x), 1e-6f);
  EXPECT_NEAR(x_phase_1.origin_x + 0.25f,
              std::round(x_phase_1.origin_x + 0.25f), 1e-6f);
  EXPECT_NEAR(y_phase_1.origin_y - 0.25f,
              std::round(y_phase_1.origin_y - 0.25f), 1e-6f);
}

TEST(FreeTypeScalerContextTest, AppliesPackedPhaseInPhysicalPixelSpace) {
  auto typeface = Typeface::MakeFromFile(kRobotoRegular);
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph_id = typeface->UnicharToGlyph('H');
  ASSERT_NE(glyph_id, 0);

  constexpr float kContentScale = 2.f;
  const RasterizedGlyph phase =
      RasterizeGlyph(typeface, glyph_id, 1, 0, kContentScale);

  ASSERT_FALSE(phase.pixels.empty());
  const float physical_origin = phase.origin_x * kContentScale + 0.25f;
  EXPECT_NEAR(physical_origin, std::round(physical_origin), 1e-6f);
}

TEST(FreeTypeScalerContextTest, DisablesHintingForNonAxisAlignedTransform) {
  auto typeface = Typeface::MakeFromFile(kRobotoRegular);
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph_id = typeface->UnicharToGlyph('H');
  ASSERT_NE(glyph_id, 0);

  const Matrix22 y_skew{1.f, 0.f, 0.25f, 1.f};
  const RasterizedGlyph requested_hinted = RasterizeGlyph(
      typeface, glyph_id, 1, 1, 1.f, y_skew, Font::FontHinting::kNormal);
  const RasterizedGlyph explicitly_unhinted = RasterizeGlyph(
      typeface, glyph_id, 1, 1, 1.f, y_skew, Font::FontHinting::kNone);

  EXPECT_FLOAT_EQ(requested_hinted.origin_x, explicitly_unhinted.origin_x);
  EXPECT_FLOAT_EQ(requested_hinted.origin_y, explicitly_unhinted.origin_y);
  EXPECT_EQ(requested_hinted.width, explicitly_unhinted.width);
  EXPECT_EQ(requested_hinted.height, explicitly_unhinted.height);
  EXPECT_EQ(requested_hinted.pixels, explicitly_unhinted.pixels);
}

TEST(FreeTypeScalerContextTest, ExpandsMonochromeBitmapToGray8) {
  uint8_t source_pixels[] = {0xA8u, 0x50u};
  FT_Bitmap source{};
  source.rows = 2;
  source.width = 5;
  source.pitch = 1;
  source.buffer = source_pixels;
  source.num_grays = 2;
  source.pixel_mode = FT_PIXEL_MODE_MONO;

  GlyphBitmapData target;
  ASSERT_TRUE(internal::CopyFreetypeBitmap(source, &target));
  ASSERT_NE(target.buffer, nullptr);
  EXPECT_EQ(target.format, BitmapFormat::kGray8);
  EXPECT_EQ(target.width, 5.f);
  EXPECT_EQ(target.height, 2.f);
  EXPECT_EQ(target.RowBytes(), 5u);
  EXPECT_TRUE(target.need_free);

  constexpr uint8_t kExpected[] = {0xFFu, 0u,    0xFFu, 0u,    0xFFu,
                                   0u,    0xFFu, 0u,    0xFFu, 0u};
  EXPECT_EQ(std::memcmp(target.buffer, kExpected, sizeof(kExpected)), 0);
  std::free(target.buffer);
}

TEST(FreeTypeScalerContextTest, CopiesNegativePitchFromLogicalTopRow) {
  uint8_t source_pixels[] = {0x50u, 0xA8u};
  FT_Bitmap source{};
  source.rows = 2;
  source.width = 5;
  source.pitch = -1;
  source.buffer = source_pixels + 1;
  source.num_grays = 2;
  source.pixel_mode = FT_PIXEL_MODE_MONO;

  GlyphBitmapData target;
  ASSERT_TRUE(internal::CopyFreetypeBitmap(source, &target));
  ASSERT_NE(target.buffer, nullptr);

  constexpr uint8_t kExpected[] = {0xFFu, 0u,    0xFFu, 0u,    0xFFu,
                                   0u,    0xFFu, 0u,    0xFFu, 0u};
  EXPECT_EQ(std::memcmp(target.buffer, kExpected, sizeof(kExpected)), 0);
  std::free(target.buffer);
}

TEST_F(TypefaceTest, GetFontDescriptorBasic) {
  FontDescriptor desc = default_typeface->GetFontDescriptor();
  EXPECT_EQ(desc.collection_index, 0);
  EXPECT_EQ(desc.family_name, "Roboto");
  EXPECT_EQ(desc.post_script_name, "Roboto");
  EXPECT_EQ(desc.full_name, "");
  EXPECT_EQ(desc.variation_position.GetCoordinates().size(), 3);
  EXPECT_EQ(desc.variation_position.GetCoordinates()[0].axis,
            SetFourByteTag('w', 'g', 'h', 't'));
  EXPECT_EQ(desc.variation_position.GetCoordinates()[0].value, 400);
  EXPECT_EQ(desc.variation_position.GetCoordinates()[1].axis,
            SetFourByteTag('w', 'd', 't', 'h'));
  EXPECT_EQ(desc.variation_position.GetCoordinates()[1].value, 100);
  EXPECT_EQ(desc.variation_position.GetCoordinates()[2].axis,
            SetFourByteTag('i', 't', 'a', 'l'));
  EXPECT_EQ(desc.variation_position.GetCoordinates()[2].value, 0);
}

TEST_F(TypefaceTest, MakeVariationDoesNotCrash) {
  FontArguments args{};
  VariationPosition position;
  position.AddCoordinate(SetFourByteTag('w', 'g', 'h', 't'), 500);
  position.AddCoordinate(SetFourByteTag('w', 'd', 't', 'h'), 100);
  position.AddCoordinate(SetFourByteTag('i', 't', 'a', 'l'), 1);
  args.SetVariationDesignPosition(position);
  auto var = default_typeface->MakeVariation(args);
  FontStyle style = var->GetFontStyle();
  EXPECT_NE(var, nullptr);
  EXPECT_EQ(style.weight(), 500);
  EXPECT_EQ(style.width(), FontStyle::Width::kNormal_Width);
  EXPECT_EQ(style.slant(), FontStyle::Slant::kItalic_Slant);
}

/**
 * Verifies that MakeVariation is thread-safe and does not crash
 * when invoked concurrently.
 */
TEST_F(TypefaceTest, MakeVariationThreadSafe) {
  ConcurrentRunner runner(kThreadCount, kIterations);

  runner.Run([&](int) {
    FontArguments args{};
    VariationPosition position;
    position.AddCoordinate(SetFourByteTag('w', 'g', 'h', 't'), 500);
    position.AddCoordinate(SetFourByteTag('w', 'd', 't', 'h'), 100);
    position.AddCoordinate(SetFourByteTag('i', 't', 'a', 'l'), 1);
    args.SetVariationDesignPosition(position);
    auto var = default_typeface->MakeVariation(args);
    FontStyle style = var->GetFontStyle();
    EXPECT_NE(var, nullptr);
    EXPECT_EQ(style.weight(), 500);
    EXPECT_EQ(style.width(), FontStyle::Width::kNormal_Width);
    EXPECT_EQ(style.slant(), FontStyle::Slant::kItalic_Slant);
  });
}
