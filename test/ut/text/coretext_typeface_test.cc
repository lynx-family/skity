// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <iterator>
#include <skity/geometry/matrix.hpp>
#include <skity/io/data.hpp>
#include <skity/text/font.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/ports/typeface_ct.hpp>
#include <skity/text/typeface.hpp>
#include <string>
#include <vector>

#include "src/text/ports/darwin/scaler_context_darwin.hpp"
#include "src/text/ports/darwin/typeface_darwin.hpp"
#include "src/text/ports/darwin/types_darwin.hpp"

namespace skity {
namespace {

constexpr const char* kNotoSansCjkTtc =
    SKITY_FONT_DIR "fonts/resources/NotoSansCJK-Regular.ttc";

void ExpectNotoSansCjkIndex1(std::shared_ptr<Typeface> const& typeface) {
  ASSERT_NE(typeface, nullptr);

  FontDescriptor desc = typeface->GetFontDescriptor();
  EXPECT_EQ(desc.collection_index, 1);
  EXPECT_NE(desc.family_name.find("Noto Sans CJK KR"), std::string::npos);
  EXPECT_NE(desc.post_script_name.find("NotoSansCJKKR-Regular"),
            std::string::npos);
  EXPECT_FALSE(desc.family_name.empty());
  EXPECT_FALSE(desc.full_name.empty());
  EXPECT_FALSE(desc.post_script_name.empty());
  EXPECT_EQ(desc.family_name.find('\0'), std::string::npos);
  EXPECT_EQ(desc.full_name.find('\0'), std::string::npos);
  EXPECT_EQ(desc.post_script_name.find('\0'), std::string::npos);
  EXPECT_NE(typeface->UnicharToGlyph(0x4E00), 0);
  EXPECT_GT(typeface->CountTables(), 0);
}

struct RasterizedGlyph {
  float origin_x = 0;
  float origin_y = 0;
  float width = 0;
  float height = 0;
  BitmapFormat format = BitmapFormat::kUnknown;
  std::vector<uint8_t> pixels;
};

size_t BytesPerPixel(BitmapFormat format) {
  switch (format) {
    case BitmapFormat::kGray8:
      return 1;
    case BitmapFormat::kBGRA8:
    case BitmapFormat::kRGBA8:
      return 4;
    case BitmapFormat::kUnknown:
      return 0;
  }
}

RasterizedGlyph RasterizeGlyph(ScalerContextDarwin* context, GlyphID glyph_id,
                               const StrokeDesc& stroke_desc) {
  GlyphData glyph(glyph_id);
  context->GetImage(&glyph, stroke_desc);

  const GlyphBitmapData& image = glyph.Image();
  RasterizedGlyph result{image.origin_x, image.origin_y, image.width,
                         image.height,   image.format,   {}};
  EXPECT_NE(image.buffer, nullptr);

  const size_t bytes_per_pixel = BytesPerPixel(image.format);
  const size_t width = static_cast<size_t>(image.width);
  const size_t height = static_cast<size_t>(image.height);
  const size_t tight_row_bytes = width * bytes_per_pixel;
  EXPECT_GT(bytes_per_pixel, 0u);
  EXPECT_GE(image.RowBytes(), tight_row_bytes);
  if (!image.buffer || bytes_per_pixel == 0 ||
      image.RowBytes() < tight_row_bytes) {
    return result;
  }

  result.pixels.resize(tight_row_bytes * height);
  for (size_t y = 0; y < height; ++y) {
    std::memcpy(result.pixels.data() + y * tight_row_bytes,
                image.buffer + y * image.RowBytes(), tight_row_bytes);
  }
  return result;
}

void ExpectSameRaster(const RasterizedGlyph& actual,
                      const RasterizedGlyph& expected) {
  EXPECT_FLOAT_EQ(actual.origin_x, expected.origin_x);
  EXPECT_FLOAT_EQ(actual.origin_y, expected.origin_y);
  EXPECT_FLOAT_EQ(actual.width, expected.width);
  EXPECT_FLOAT_EQ(actual.height, expected.height);
  EXPECT_EQ(actual.format, expected.format);
  EXPECT_EQ(actual.pixels, expected.pixels);
}

TEST(CoreTextTypefaceTest, ConvertsCFStringToExactUtf8Length) {
  constexpr UniChar kCharacters[] = {'P', 'i', 'n', 'g', 'F',    'a',   'n',
                                     'g', 'S', 'C', '-', 0x4E2D, 0x6587};
  UniqueCFRef<CFStringRef> cf_string(CFStringCreateWithCharacters(
      kCFAllocatorDefault, kCharacters, std::size(kCharacters)));
  ASSERT_NE(cf_string, nullptr);

  std::string converted = cf_string_to_string(cf_string.get());

  EXPECT_EQ(converted, "PingFangSC-\xE4\xB8\xAD\xE6\x96\x87");
  EXPECT_EQ(converted.find('\0'), std::string::npos);
}

TEST(CoreTextTypefaceTest, MakeFromFileSupportsTtcIndex) {
  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 1);

  ExpectNotoSansCjkIndex1(typeface);
}

TEST(CoreTextTypefaceTest, MakeFromDataSupportsTtcIndex) {
  auto data = Data::MakeFromFileName(kNotoSansCjkTtc);
  ASSERT_NE(data, nullptr);
  ASSERT_GT(data->Size(), 0u);

  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromData(data, 1);

  ExpectNotoSansCjkIndex1(typeface);
}

TEST(CoreTextTypefaceTest, MakeFromDataRejectsInvalidTtcIndex) {
  auto data = Data::MakeFromFileName(kNotoSansCjkTtc);
  ASSERT_NE(data, nullptr);
  ASSERT_GT(data->Size(), 0u);

  auto font_manager = FontManager::RefDefault();
  EXPECT_EQ(font_manager->MakeFromData(data, -1), nullptr);
  EXPECT_EQ(font_manager->MakeFromData(data, 999), nullptr);
}

TEST(CoreTextTypefaceTest, MakeVariationRejectsTtcIndexMismatch) {
  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(typeface, nullptr);

  FontArguments args;
  args.SetCollectionIndex(1);

  EXPECT_EQ(typeface->MakeVariation(args), nullptr);
}

TEST(CoreTextTypefaceTest, FromCTFontWithoutCacheCreatesIndependentTypefaces) {
  CTFontRef ct_font =
      CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 16.0, nullptr);
  ASSERT_NE(ct_font, nullptr);

  auto first = TypefaceCT::TypefaceFromCTFontWithoutCache(ct_font);
  auto second = TypefaceCT::TypefaceFromCTFontWithoutCache(ct_font);
  CFRelease(ct_font);

  ASSERT_NE(first, nullptr);
  ASSERT_NE(second, nullptr);
  EXPECT_NE(first->TypefaceId(), second->TypefaceId());
}

TEST(CoreTextScalerContextTest, ReusesOffscreenResources) {
  OffScreenContext context(Color_BLACK);

  auto color_target = context.PrepareContext(32, 32, true);
  ASSERT_TRUE(color_target);
  EXPECT_TRUE(color_target.context_was_created);
  EXPECT_EQ(color_target.active_width, 32u);
  EXPECT_EQ(color_target.active_height, 32u);
  EXPECT_EQ(color_target.context_width, 32u);
  EXPECT_EQ(color_target.context_height, 32u);
  EXPECT_EQ(color_target.row_bytes, 32u * 4u);
  void* initial_buffer = color_target.storage;
  ASSERT_NE(initial_buffer, nullptr);
  EXPECT_EQ(color_target.pixels, initial_buffer);

  auto gray_target = context.PrepareContext(16, 16, false);
  ASSERT_TRUE(gray_target);
  EXPECT_TRUE(gray_target.context_was_created);
  EXPECT_EQ(gray_target.active_width, 16u);
  EXPECT_EQ(gray_target.active_height, 16u);
  EXPECT_EQ(gray_target.context_width, 32u);
  EXPECT_EQ(gray_target.context_height, 32u);
  EXPECT_EQ(gray_target.row_bytes, 32u);
  EXPECT_EQ(gray_target.storage, initial_buffer);
  EXPECT_EQ(gray_target.pixels,
            static_cast<uint8_t*>(initial_buffer) + 16u * 32u);

  CGContextRef gray_context = gray_target.context;
  auto smaller_gray_target = context.PrepareContext(8, 8, false);
  ASSERT_TRUE(smaller_gray_target);
  EXPECT_FALSE(smaller_gray_target.context_was_created);
  EXPECT_EQ(smaller_gray_target.context, gray_context);
  EXPECT_EQ(smaller_gray_target.storage, initial_buffer);

  auto grown_target = context.PrepareContext(64, 64, false);
  ASSERT_TRUE(grown_target);
  EXPECT_TRUE(grown_target.context_was_created);
  EXPECT_EQ(grown_target.context_width, 64u);
  EXPECT_EQ(grown_target.context_height, 64u);
  void* grown_buffer = grown_target.storage;
  ASSERT_NE(grown_buffer, nullptr);
  EXPECT_EQ(grown_buffer, initial_buffer);

  auto expanded_target = context.PrepareContext(128, 128, false);
  ASSERT_TRUE(expanded_target);
  EXPECT_TRUE(expanded_target.context_was_created);
  EXPECT_EQ(expanded_target.context_width, 128u);
  EXPECT_EQ(expanded_target.context_height, 128u);
  void* expanded_buffer = expanded_target.storage;
  ASSERT_NE(expanded_buffer, nullptr);
  EXPECT_NE(expanded_buffer, initial_buffer);

  CGContextRef expanded_context = expanded_target.context;
  auto reused_target = context.PrepareContext(32, 32, false);
  ASSERT_TRUE(reused_target);
  EXPECT_FALSE(reused_target.context_was_created);
  EXPECT_EQ(reused_target.context, expanded_context);
  EXPECT_EQ(reused_target.storage, expanded_buffer);
}

TEST(CoreTextScalerContextTest,
     StableCapacityCreatesOneBitmapContextForThousandGlyphs) {
  OffScreenContext context(Color_BLACK);
  CGContextRef reused_context = nullptr;
  size_t creation_count = 0;

  for (size_t i = 0; i < 1000; ++i) {
    auto target = context.PrepareContext(31, 29, false);
    ASSERT_TRUE(target);
    creation_count += target.context_was_created ? 1u : 0u;

    if (reused_context == nullptr) {
      reused_context = target.context;
    } else {
      EXPECT_EQ(target.context, reused_context);
    }
  }

  EXPECT_EQ(creation_count, 1u);
}

TEST(CoreTextScalerContextTest, ClearsOnlyActiveGlyphRect) {
  constexpr uint8_t kSentinel = 0xA5;
  constexpr uint32_t kContextWidth = 16;
  constexpr uint32_t kContextHeight = 16;
  constexpr uint32_t kPaddedActiveWidth = 5;
  constexpr uint32_t kActiveHeight = 7;
  const bool color_modes[] = {false, true};
  const uint32_t active_widths[] = {kPaddedActiveWidth, kContextWidth};

  for (bool need_color : color_modes) {
    SCOPED_TRACE(need_color ? "BGRA8" : "A8");
    OffScreenContext context(Color_BLACK);
    auto full_target =
        context.PrepareContext(kContextWidth, kContextHeight, need_color);
    ASSERT_TRUE(full_target);

    const size_t storage_size =
        full_target.row_bytes * full_target.context_height;

    for (uint32_t active_width : active_widths) {
      SCOPED_TRACE(active_width);
      std::memset(full_target.storage, kSentinel, storage_size);

      auto active_target =
          context.PrepareContext(active_width, kActiveHeight, need_color);
      ASSERT_TRUE(active_target);
      EXPECT_FALSE(active_target.context_was_created);
      EXPECT_EQ(active_target.active_width, active_width);
      EXPECT_EQ(active_target.active_height, kActiveHeight);
      EXPECT_EQ(active_target.context_width, kContextWidth);
      EXPECT_EQ(active_target.context_height, kContextHeight);

      const size_t bytes_per_pixel = need_color ? 4 : 1;
      const size_t active_row_bytes = active_width * bytes_per_pixel;
      const size_t first_active_row = kContextHeight - kActiveHeight;
      EXPECT_EQ(
          active_target.pixels,
          active_target.storage + first_active_row * active_target.row_bytes);

      for (size_t y = 0; y < kContextHeight; ++y) {
        const uint8_t* row =
            active_target.storage + y * active_target.row_bytes;
        for (size_t x = 0; x < active_target.row_bytes; ++x) {
          const bool is_active_pixel =
              y >= first_active_row && x < active_row_bytes;
          EXPECT_EQ(row[x], is_active_pixel ? 0 : kSentinel)
              << "storage byte (" << x << ", " << y << ")";
        }
      }
    }
  }
}

TEST(CoreTextScalerContextTest, CachesAndInvalidatesDrawState) {
  OffScreenContext context(Color_BLACK);
  auto first_target = context.PrepareContext(16, 16, false);
  ASSERT_TRUE(first_target);

  EXPECT_TRUE(context.SetTextDrawingMode(kCGTextStroke));
  EXPECT_FALSE(context.SetTextDrawingMode(kCGTextStroke));
  EXPECT_TRUE(context.SetTextDrawingMode(kCGTextFill));
  EXPECT_FALSE(context.SetTextDrawingMode(kCGTextFill));

  EXPECT_TRUE(context.SetLineWidth(2.5));
  EXPECT_FALSE(context.SetLineWidth(2.5));
  EXPECT_TRUE(context.SetLineWidth(3.5));
  EXPECT_FALSE(context.SetLineWidth(3.5));

  EXPECT_TRUE(context.SetLineCap(kCGLineCapRound));
  EXPECT_FALSE(context.SetLineCap(kCGLineCapRound));
  EXPECT_TRUE(context.SetLineJoin(kCGLineJoinBevel));
  EXPECT_FALSE(context.SetLineJoin(kCGLineJoinBevel));
  EXPECT_TRUE(context.SetMiterLimit(7.0));
  EXPECT_FALSE(context.SetMiterLimit(7.0));
  EXPECT_TRUE(context.SetShouldAntialias(true));
  EXPECT_FALSE(context.SetShouldAntialias(true));

  auto reused_target = context.PrepareContext(8, 8, false);
  ASSERT_TRUE(reused_target);
  EXPECT_FALSE(reused_target.context_was_created);
  EXPECT_FALSE(context.SetTextDrawingMode(kCGTextFill));
  EXPECT_FALSE(context.SetLineWidth(3.5));
  EXPECT_FALSE(context.SetLineCap(kCGLineCapRound));
  EXPECT_FALSE(context.SetLineJoin(kCGLineJoinBevel));
  EXPECT_FALSE(context.SetMiterLimit(7.0));
  EXPECT_FALSE(context.SetShouldAntialias(true));

  auto grown_target = context.PrepareContext(32, 32, false);
  ASSERT_TRUE(grown_target);
  EXPECT_TRUE(grown_target.context_was_created);
  EXPECT_TRUE(context.SetTextDrawingMode(kCGTextFill));
  EXPECT_TRUE(context.SetLineWidth(3.5));
  EXPECT_TRUE(context.SetLineCap(kCGLineCapRound));
  EXPECT_TRUE(context.SetLineJoin(kCGLineJoinBevel));
  EXPECT_TRUE(context.SetMiterLimit(7.0));
  EXPECT_TRUE(context.SetShouldAntialias(true));
}

TEST(CoreTextScalerContextTest, EdgingControlsAntialiasing) {
  auto font_manager = FontManager::RefDefault();
  auto base_typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(base_typeface, nullptr);
  auto typeface = std::static_pointer_cast<TypefaceDarwin>(base_typeface);
  const GlyphID glyph_id = typeface->UnicharToGlyph('A');
  ASSERT_NE(glyph_id, 0);

  Font alias_font(typeface, 48.f);
  alias_font.SetEdging(Font::Edging::kAlias);
  ScalerContextDesc alias_desc =
      ScalerContextDesc::MakeTransformed(alias_font, Paint(), 1.f, Matrix22{});

  Font antialias_font(typeface, 48.f);
  antialias_font.SetEdging(Font::Edging::kAntiAlias);
  ScalerContextDesc antialias_desc = ScalerContextDesc::MakeTransformed(
      antialias_font, Paint(), 1.f, Matrix22{});

  const StrokeDesc fill_desc{false, 0.f, Paint::kDefault_Cap,
                             Paint::kDefault_Join, Paint::kDefaultMiterLimit};
  ScalerContextDarwin alias_context(typeface, &alias_desc);
  ScalerContextDarwin antialias_context(typeface, &antialias_desc);
  const RasterizedGlyph alias =
      RasterizeGlyph(&alias_context, glyph_id, fill_desc);
  const RasterizedGlyph antialias =
      RasterizeGlyph(&antialias_context, glyph_id, fill_desc);

  ASSERT_EQ(alias.format, BitmapFormat::kGray8);
  ASSERT_EQ(antialias.format, BitmapFormat::kGray8);
  ASSERT_FALSE(alias.pixels.empty());
  ASSERT_FALSE(antialias.pixels.empty());
  EXPECT_NE(alias.pixels, antialias.pixels);
  EXPECT_TRUE(
      std::all_of(alias.pixels.begin(), alias.pixels.end(),
                  [](uint8_t alpha) { return alpha == 0 || alpha == 255; }));
  EXPECT_TRUE(
      std::any_of(antialias.pixels.begin(), antialias.pixels.end(),
                  [](uint8_t alpha) { return alpha != 0 && alpha != 255; }));
}

TEST(CoreTextScalerContextTest, ReusedContextDrawStateMatchesFreshRendering) {
  auto font_manager = FontManager::RefDefault();
  auto base_typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(base_typeface, nullptr);
  auto typeface = std::static_pointer_cast<TypefaceDarwin>(base_typeface);

  Font font(typeface, 64.f);
  ScalerContextDesc desc =
      ScalerContextDesc::MakeTransformed(font, Paint(), 1.5f, Matrix22{});
  const GlyphID wide_glyph = typeface->UnicharToGlyph('W');
  const GlyphID small_glyph = typeface->UnicharToGlyph('.');
  ASSERT_NE(wide_glyph, 0);
  ASSERT_NE(small_glyph, 0);

  const StrokeDesc fill_desc{false, 0.f, Paint::kDefault_Cap,
                             Paint::kDefault_Join, Paint::kDefaultMiterLimit};
  const StrokeDesc stroke_a{true, 3.5f, Paint::kRound_Cap, Paint::kBevel_Join,
                            5.f};
  const StrokeDesc stroke_b{true, 6.f, Paint::kSquare_Cap, Paint::kMiter_Join,
                            8.f};

  ScalerContextDarwin fresh_fill(typeface, &desc);
  const RasterizedGlyph expected_fill =
      RasterizeGlyph(&fresh_fill, wide_glyph, fill_desc);
  ScalerContextDarwin fresh_stroke_a(typeface, &desc);
  const RasterizedGlyph expected_stroke_a =
      RasterizeGlyph(&fresh_stroke_a, wide_glyph, stroke_a);
  ScalerContextDarwin fresh_stroke_b(typeface, &desc);
  const RasterizedGlyph expected_stroke_b =
      RasterizeGlyph(&fresh_stroke_b, wide_glyph, stroke_b);
  ASSERT_FALSE(expected_fill.pixels.empty());
  ASSERT_FALSE(expected_stroke_a.pixels.empty());
  ASSERT_FALSE(expected_stroke_b.pixels.empty());

  ScalerContextDarwin reused(typeface, &desc);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, stroke_b),
                   expected_stroke_b);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc),
                   expected_fill);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, stroke_a),
                   expected_stroke_a);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, stroke_b),
                   expected_stroke_b);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc),
                   expected_fill);

  ScalerContextDarwin rebuilt(typeface, &desc);
  RasterizeGlyph(&rebuilt, small_glyph, stroke_a);
  ExpectSameRaster(RasterizeGlyph(&rebuilt, wide_glyph, stroke_a),
                   expected_stroke_a);
}

TEST(CoreTextScalerContextTest, ReusedFakeBoldStateMatchesFreshRendering) {
  auto font_manager = FontManager::RefDefault();
  auto base_typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(base_typeface, nullptr);
  auto typeface = std::static_pointer_cast<TypefaceDarwin>(base_typeface);

  Font font(typeface, 64.f);
  font.SetEmbolden(true);
  ScalerContextDesc desc =
      ScalerContextDesc::MakeTransformed(font, Paint(), 1.25f, Matrix22{});
  const GlyphID wide_glyph = typeface->UnicharToGlyph('W');
  const GlyphID small_glyph = typeface->UnicharToGlyph('.');
  ASSERT_NE(wide_glyph, 0);
  ASSERT_NE(small_glyph, 0);

  const StrokeDesc fill_desc{false, 0.f, Paint::kDefault_Cap,
                             Paint::kDefault_Join, Paint::kDefaultMiterLimit};
  ScalerContextDarwin fresh(typeface, &desc);
  const RasterizedGlyph expected =
      RasterizeGlyph(&fresh, wide_glyph, fill_desc);
  ASSERT_FALSE(expected.pixels.empty());

  ScalerContextDarwin reused(typeface, &desc);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc), expected);
  RasterizeGlyph(&reused, small_glyph, fill_desc);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc), expected);
}

TEST(CoreTextScalerContextTest, ReusedTransformedContextMatchesFreshRendering) {
  auto font_manager = FontManager::RefDefault();
  auto base_typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(base_typeface, nullptr);
  auto typeface = std::static_pointer_cast<TypefaceDarwin>(base_typeface);

  Font font(typeface, 64.f);
  const Matrix22 transform{0.95f, 0.2f, -0.15f, 1.1f};
  ScalerContextDesc desc =
      ScalerContextDesc::MakeTransformed(font, Paint(), 1.25f, transform);
  const GlyphID wide_glyph = typeface->UnicharToGlyph('W');
  const GlyphID small_glyph = typeface->UnicharToGlyph('.');
  ASSERT_NE(wide_glyph, 0);
  ASSERT_NE(small_glyph, 0);

  const StrokeDesc fill_desc{false, 0.f, Paint::kDefault_Cap,
                             Paint::kDefault_Join, Paint::kDefaultMiterLimit};
  ScalerContextDarwin fresh(typeface, &desc);
  const RasterizedGlyph expected =
      RasterizeGlyph(&fresh, wide_glyph, fill_desc);
  ASSERT_FALSE(expected.pixels.empty());

  ScalerContextDarwin reused(typeface, &desc);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc), expected);
  RasterizeGlyph(&reused, small_glyph, fill_desc);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc), expected);
  ExpectSameRaster(RasterizeGlyph(&reused, wide_glyph, fill_desc), expected);
}

TEST(CoreTextScalerContextTest, ReusedColorGlyphPreservesBgraAndAlpha) {
  auto font_manager = FontManager::RefDefault();
  auto base_typeface =
      font_manager->MatchFamilyStyle("Apple Color Emoji", FontStyle::Normal());
  if (!base_typeface || !base_typeface->ContainsColorTable()) {
    GTEST_SKIP() << "Apple Color Emoji is unavailable";
  }
  auto typeface = std::static_pointer_cast<TypefaceDarwin>(base_typeface);

  Font font(typeface, 64.f);
  ScalerContextDesc desc =
      ScalerContextDesc::MakeTransformed(font, Paint(), 1.f, Matrix22{});
  const GlyphID grinning_glyph = typeface->UnicharToGlyph(0x1F600);
  const GlyphID heart_glyph = typeface->UnicharToGlyph(0x2764);
  ASSERT_NE(grinning_glyph, 0);

  const StrokeDesc fill_desc{false, 0.f, Paint::kDefault_Cap,
                             Paint::kDefault_Join, Paint::kDefaultMiterLimit};
  ScalerContextDarwin fresh(typeface, &desc);
  const RasterizedGlyph expected =
      RasterizeGlyph(&fresh, grinning_glyph, fill_desc);
  ASSERT_EQ(expected.format, BitmapFormat::kBGRA8);
  ASSERT_FALSE(expected.pixels.empty());

  bool has_alpha = false;
  for (size_t i = 3; i < expected.pixels.size(); i += 4) {
    has_alpha |= expected.pixels[i] != 0;
  }
  EXPECT_TRUE(has_alpha);

  ScalerContextDarwin reused(typeface, &desc);
  ExpectSameRaster(RasterizeGlyph(&reused, grinning_glyph, fill_desc),
                   expected);
  if (heart_glyph != 0) {
    RasterizeGlyph(&reused, heart_glyph, fill_desc);
  }
  ExpectSameRaster(RasterizeGlyph(&reused, grinning_glyph, fill_desc),
                   expected);
}

TEST(CoreTextScalerContextTest,
     SingleGlyphBitmapLoadsCopyBeforeOffscreenReuse) {
  auto font_manager = FontManager::RefDefault();
  auto typeface = font_manager->MakeFromFile(kNotoSansCjkTtc, 0);
  ASSERT_NE(typeface, nullptr);

  Font font(typeface, 64.f);
  const GlyphID wide_glyph = typeface->UnicharToGlyph('W');
  const GlyphID small_glyph = typeface->UnicharToGlyph('.');
  ASSERT_NE(wide_glyph, 0);
  ASSERT_NE(small_glyph, 0);

  const GlyphData* wide_data = nullptr;
  font.LoadGlyphBitmap(&wide_glyph, 1, &wide_data, Paint(), 1.f, Matrix());
  ASSERT_NE(wide_data, nullptr);
  GlyphBitmapData first_image = wide_data->Image();
  ASSERT_NE(first_image.buffer, nullptr);
  ASSERT_FALSE(first_image.need_free);
  ASSERT_EQ(first_image.format, BitmapFormat::kGray8);
  const size_t tight_row_bytes = static_cast<size_t>(first_image.width);
  const size_t image_height = static_cast<size_t>(first_image.height);
  ASSERT_GE(first_image.row_bytes, tight_row_bytes);
  std::vector<uint8_t> first_pixels(tight_row_bytes * image_height);
  for (size_t y = 0; y < image_height; ++y) {
    std::memcpy(first_pixels.data() + y * tight_row_bytes,
                first_image.buffer + y * first_image.RowBytes(),
                tight_row_bytes);
  }
  EXPECT_TRUE(std::any_of(first_pixels.begin(), first_pixels.end(),
                          [](uint8_t alpha) { return alpha != 0; }));

  const GlyphData* small_data = nullptr;
  font.LoadGlyphBitmap(&small_glyph, 1, &small_data, Paint(), 1.f, Matrix());
  ASSERT_NE(small_data, nullptr);
  const GlyphBitmapData small_image = small_data->Image();
  ASSERT_NE(small_image.buffer, nullptr);
  ASSERT_FALSE(small_image.need_free);
  ASSERT_EQ(small_image.format, BitmapFormat::kGray8);
  ASSERT_EQ(small_image.RowBytes(), first_image.RowBytes());
  bool small_image_has_coverage = false;
  for (size_t y = 0; y < static_cast<size_t>(small_image.height); ++y) {
    const uint8_t* row = small_image.buffer + y * small_image.RowBytes();
    small_image_has_coverage |=
        std::any_of(row, row + static_cast<size_t>(small_image.width),
                    [](uint8_t alpha) { return alpha != 0; });
  }
  EXPECT_TRUE(small_image_has_coverage);

  font.LoadGlyphBitmap(&wide_glyph, 1, &wide_data, Paint(), 1.f, Matrix());
  ASSERT_NE(wide_data, nullptr);
  GlyphBitmapData second_image = wide_data->Image();
  ASSERT_NE(second_image.buffer, nullptr);
  ASSERT_FALSE(second_image.need_free);
  ASSERT_EQ(second_image.width, first_image.width);
  ASSERT_EQ(second_image.height, first_image.height);
  EXPECT_EQ(second_image.buffer, first_image.buffer);
  EXPECT_EQ(second_image.RowBytes(), first_image.RowBytes());
  std::vector<uint8_t> second_pixels(tight_row_bytes * image_height);
  for (size_t y = 0; y < image_height; ++y) {
    std::memcpy(second_pixels.data() + y * tight_row_bytes,
                second_image.buffer + y * second_image.RowBytes(),
                tight_row_bytes);
  }
  EXPECT_EQ(second_pixels, first_pixels);
}

}  // namespace
}  // namespace skity
