// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <thread>
#include <utility>

#include "src/text/scaler_context_container.hpp"

namespace skity {
namespace {

class ImageTestScalerContext final : public ScalerContext {
 public:
  explicit ImageTestScalerContext(const ScalerContextDesc* desc,
                                  bool allocate = false)
      : ScalerContext({}, desc), allocate_(allocate) {}

  GlyphBitmapData layout = [] {
    GlyphBitmapData image;
    image.origin_x = -2.f;
    image.origin_y = 5.f;
    image.width = 3.f;
    image.height = 2.f;
    image.row_bytes = 8;
    image.format = BitmapFormat::kGray8;
    return image;
  }();
  uint8_t* last_buffer = nullptr;
  bool fail = false;

 protected:
  void GenerateMetrics(GlyphData*) override {}
  void GenerateImage(PackedGlyphID, GlyphData* glyph,
                     const StrokeDesc&) override {
    if (fail) {
      return;
    }
    auto image = layout;
    last_buffer =
        allocate_ ? static_cast<uint8_t*>(std::malloc(16)) : scratch_.data();
    std::memset(last_buffer, ++requests_, allocate_ ? 16 : scratch_.size());
    image.buffer = last_buffer;
    image.need_free = allocate_;
    const_cast<GlyphBitmapData&>(glyph->Image()) = image;
  }
  void GenerateImageInfo(PackedGlyphID, GlyphData*,
                         const StrokeDesc&) override {}
  bool GeneratePath(GlyphData* glyph) override {
    const_cast<Path&>(glyph->GetPath()).AddRect(0.f, 0.f, 3.f, 2.f);
    return true;
  }
  void GenerateFontMetrics(FontMetrics* metrics) override { *metrics = {}; }
  uint16_t OnGetFixedSize() override { return 0; }

 private:
  bool allocate_;
  uint8_t requests_ = 0;
  // Two 3-byte rows at stride 8, with no padding after the last row.
  std::array<uint8_t, 11> scratch_{};
};

TEST(GlyphImageTest,
     BorrowedPixelsSurviveLaterRequestsAndContainerDestruction) {
  OwnedGlyphImage first;
  {
    ScalerContextDesc desc{};
    auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
    auto* backend = scaler.get();
    ScalerContextContainer container(std::move(scaler));
    first = container.PrepareOwnedImage(PackedGlyphID(7, 1, 2), Paint());
    EXPECT_NE(first.Image().buffer, backend->last_buffer);
    auto second = container.PrepareOwnedImage(PackedGlyphID(7, 1, 2), Paint());
    EXPECT_NE(first.Get(), second.Get());
    EXPECT_NE(first.Image().buffer, second.Image().buffer);
    EXPECT_EQ(first.Image().buffer[0], 1);
    EXPECT_EQ(second.Image().buffer[0], 2);
    second.Image().buffer[0] = 99;
    EXPECT_EQ(first.Image().buffer[0], 1);
  }
  EXPECT_EQ(first.Get()->Id(), 7);
  EXPECT_FALSE(first.Image().need_free);
  EXPECT_EQ(first.Image().RowBytes(), 8u);
  EXPECT_EQ(first.Image().buffer[8], 1);
  EXPECT_FLOAT_EQ(first.Image().origin_x, -2.f);
  EXPECT_FLOAT_EQ(first.Image().origin_y, 5.f);
  auto moved = std::move(first);
  EXPECT_EQ(first.Get(), nullptr);
  EXPECT_EQ(moved.Image().buffer[8], 1);
}

TEST(GlyphImageTest, AdoptsAllocatedPixelsWithoutCopying) {
  OwnedGlyphImage image;
  {
    ScalerContextDesc desc{};
    auto scaler = std::make_unique<ImageTestScalerContext>(&desc, true);
    auto* backend = scaler.get();
    ScalerContextContainer container(std::move(scaler));
    image = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
    EXPECT_EQ(image.Image().buffer, backend->last_buffer);
    EXPECT_FALSE(image.Image().need_free);
  }
  EXPECT_EQ(image.Image().buffer[0], 1);
  EXPECT_EQ(image.Image().buffer[8], 1);
}

TEST(GlyphImageTest, CopiesTightBorrowedPixels) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
  scaler->layout.row_bytes = 3;
  ScalerContextContainer container(std::move(scaler));
  auto first = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  auto second = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  EXPECT_EQ(first.Image().RowBytes(), 3u);
  for (size_t i = 0; i < 6; ++i) {
    EXPECT_EQ(first.Image().buffer[i], 1);
    EXPECT_EQ(second.Image().buffer[i], 2);
  }
}

TEST(GlyphImageTest, InitializesOnlyDestinationPadding) {
  ScalerContextDesc desc{};
  ScalerContextContainer container(
      std::make_unique<ImageTestScalerContext>(&desc));
  auto image = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  ASSERT_EQ(image.Image().RowBytes(), 8u);
  for (size_t y = 0; y < 2; ++y) {
    for (size_t x = 0; x < 8; ++x) {
      EXPECT_EQ(image.Image().buffer[y * 8 + x], x < 3 ? 1 : 0);
    }
  }
}

TEST(GlyphImageTest, MovesPreservePathStorageAndPixelOwnership) {
  ScalerContextDesc desc{};
  ScalerContextContainer container(
      std::make_unique<ImageTestScalerContext>(&desc, true));
  const GlyphID id = 7;
  const GlyphData* cached = nullptr;
  container.PreparePaths(&id, 1, &cached);
  auto first = container.PrepareOwnedImage(PackedGlyphID(id), Paint());
  const auto* points = first.Get()->GetPath().Points();
  auto* pixels = first.Image().buffer;
  ASSERT_GT(first.Get()->GetPath().CountPoints(), 0u);
  EXPECT_NE(points, cached->GetPath().Points());
  auto moved = std::move(first);
  EXPECT_EQ(first.Get(), nullptr);
  EXPECT_EQ(moved.Get()->GetPath().Points(), points);
  auto destination = container.PrepareOwnedImage(PackedGlyphID(id), Paint());
  destination = std::move(moved);
  EXPECT_EQ(moved.Get(), nullptr);
  EXPECT_EQ(destination.Get()->GetPath().Points(), points);
  EXPECT_EQ(destination.Image().buffer, pixels);
  EXPECT_EQ(destination.Image().buffer[0], 1);
  destination = OwnedGlyphImage{};
  EXPECT_EQ(destination.Get(), nullptr);
}

TEST(GlyphImageTest, LegacyBitmapIsStableAcrossThreads) {
  ScalerContextDesc desc{};
  ScalerContextContainer container(
      std::make_unique<ImageTestScalerContext>(&desc));
  const PackedGlyphID id(7);
  const GlyphData* first = nullptr;
  container.PrepareImages(&id, 1, &first, Paint());
  ASSERT_NE(first, nullptr);
  ASSERT_EQ(first->Image().buffer[0], 1);
  // Force the formerly unsafe interleaving without racing the test itself:
  // another thread regenerates this same glyph before the first consumes it.
  std::thread other([&] {
    const GlyphData* second = nullptr;
    container.PrepareImages(&id, 1, &second, Paint());
    EXPECT_EQ(second->Image().buffer[0], 2);
  });
  other.join();
  EXPECT_EQ(first->Image().buffer[0], 1);
  EXPECT_EQ(first->Image().buffer[8], 1);
}

TEST(GlyphImageTest, FailedGenerationDoesNotReusePreviousImage) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc, true);
  auto* backend = scaler.get();
  ScalerContextContainer container(std::move(scaler));
  auto first = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  backend->fail = true;
  auto failed = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  EXPECT_EQ(failed.Image().buffer, nullptr);
  EXPECT_FALSE(failed.Image().need_free);
  EXPECT_EQ(first.Image().buffer[0], 1);
}

TEST(GlyphImageTest, RejectsInvalidBorrowedImageLayouts) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc);
  auto* backend = scaler.get();
  ScalerContextContainer container(std::move(scaler));
  const auto valid = backend->layout;
  for (float width : {-1.f, 1.5f, std::numeric_limits<float>::infinity(),
                      std::numeric_limits<float>::quiet_NaN(),
                      std::numeric_limits<float>::max()}) {
    backend->layout = valid;
    backend->layout.width = width;
    EXPECT_EQ(
        container.PrepareOwnedImage(PackedGlyphID(7), Paint()).Image().buffer,
        nullptr);
  }
  backend->layout = valid;
  backend->layout.row_bytes = 2;
  EXPECT_EQ(
      container.PrepareOwnedImage(PackedGlyphID(7), Paint()).Image().buffer,
      nullptr);
  backend->layout.row_bytes = std::numeric_limits<size_t>::max();
  EXPECT_EQ(
      container.PrepareOwnedImage(PackedGlyphID(7), Paint()).Image().buffer,
      nullptr);
}

TEST(GlyphImageTest, EmptyImageDoesNotExposeZeroSizeAllocation) {
  ScalerContextDesc desc{};
  auto scaler = std::make_unique<ImageTestScalerContext>(&desc, true);
  scaler->layout.width = 0;
  scaler->layout.height = 0;
  ScalerContextContainer container(std::move(scaler));
  auto image = container.PrepareOwnedImage(PackedGlyphID(7), Paint());
  EXPECT_EQ(image.Image().buffer, nullptr);
  EXPECT_FALSE(image.Image().need_free);
  EXPECT_FLOAT_EQ(image.Image().origin_x, -2.f);
}

}  // namespace
}  // namespace skity
