// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <vector>

#include "common/golden_test_env.hpp"
#include "module/io/src/io/memory_read.hpp"
#include "module/io/src/io/memory_writer.hpp"
#include "module/io/src/picture.hpp"
#include "module/io/src/stream/stream.hpp"

namespace skity {
std::shared_ptr<Flattenable> ReadImageFilterFromMemory(
    const std::string& factory, ReadBuffer& buffer);
}  // namespace skity

namespace {

class VectorStream : public skity::WriteStream {
 public:
  bool Write(const void* buffer, size_t size) override {
    const auto* bytes = static_cast<const uint8_t*>(buffer);
    data.insert(data.end(), bytes, bytes + size);
    return true;
  }
  bool Flush() override { return true; }
  size_t BytesWritten() const override { return data.size(); }
  std::vector<uint8_t> data;
};

std::vector<uint8_t> Bytes(skity::BinaryWriteBuffer& writer) {
  VectorStream stream;
  writer.WriteToStream(stream);
  return std::move(stream.data);
}

class ScopedTarget {
 public:
  explicit ScopedTarget(std::shared_ptr<const skity::ColorSpace> space)
      : env_(skity::testing::GoldenTestEnv::GetInstance()),
        old_space_(env_->GetTargetColorSpace()),
        old_samples_(env_->GetSampleCount()) {
    env_->SetTargetColorSpace(std::move(space));
    env_->SetSampleCount(1);
  }
  ~ScopedTarget() {
    env_->SetTargetColorSpace(old_space_);
    env_->SetSampleCount(old_samples_);
  }
  skity::testing::GoldenTestEnv* env() const { return env_; }

 private:
  skity::testing::GoldenTestEnv* env_;
  std::shared_ptr<const skity::ColorSpace> old_space_;
  uint32_t old_samples_;
};

// A solid unpremul image declared in `space`; the decode path is the
// reference consumer for declared-space colors.
std::shared_ptr<skity::Image> SolidSpaceImage(
    skity::Color4f color, std::shared_ptr<const skity::ColorSpace> space) {
  auto pixmap = std::make_shared<skity::Pixmap>(32, 64);
  skity::Bitmap bitmap(pixmap, false);
  auto c = skity::ColorSetARGB(static_cast<uint8_t>(color.a * 255.f + 0.5f),
                               static_cast<uint8_t>(color.r * 255.f + 0.5f),
                               static_cast<uint8_t>(color.g * 255.f + 0.5f),
                               static_cast<uint8_t>(color.b * 255.f + 0.5f));
  for (uint32_t y = 0; y < 64; ++y) {
    for (uint32_t x = 0; x < 32; ++x) {
      bitmap.SetPixel(x, y, c);
    }
  }
  pixmap->SetColorInfo(skity::kUnpremul_AlphaType, skity::ColorType::kRGBA,
                       std::move(space));
  return skity::Image::MakeImage(pixmap);
}

void ExpectRGBA(skity::Color actual, skity::Color expected, int tolerance) {
  EXPECT_NEAR(ColorGetR(actual), ColorGetR(expected), tolerance);
  EXPECT_NEAR(ColorGetG(actual), ColorGetG(expected), tolerance);
  EXPECT_NEAR(ColorGetB(actual), ColorGetB(expected), tolerance);
  EXPECT_NEAR(ColorGetA(actual), ColorGetA(expected), tolerance);
}

}  // namespace

// Declared shadow colors must land on the same pixels as the reference
// image path: {wide-gamut primary, in-gamut mid} x {opaque, semi} on an
// sRGB target. The semi-transparent wide-gamut case is the only probe for
// premultiplication range mistakes.
TEST(ColorSpaceDropShadowGPU, DeclaredShadowMatchesImageOnSRGBTarget) {
  ASSERT_NE(skity::testing::GoldenTestEnv::GetInstance(), nullptr);
  ScopedTarget target(nullptr);
  const skity::Color4f cases[] = {
      {1.f, 0.f, 0.f, 1.f},     // P3 red, opaque
      {1.f, 0.f, 0.f, 0.5f},    // P3 red, semi-transparent
      {0.7f, 0.4f, 0.2f, 1.f},  // in-gamut mid, opaque
      {0.7f, 0.4f, 0.2f, 0.5f}  // in-gamut mid, semi-transparent
  };
  for (const auto& color : cases) {
    SCOPED_TRACE(color.r);
    auto texture =
        target.env()->RenderToTexture(128, 64, [&](skity::Canvas* c) {
          c->Clear(skity::Color_TRANSPARENT);
          skity::Paint paint;
          paint.SetColor(skity::Color_WHITE);
          paint.SetImageFilter(skity::ImageFilters::DropShadow(
              64, 0, 3, 3, color, nullptr,
              skity::DropShadowOptions{skity::ColorSpace::DisplayP3()}));
          c->DrawRect(skity::Rect::MakeWH(32, 64), paint);
          // Reference: same declared color through the image decode path.
          c->DrawImage(SolidSpaceImage(color, skity::ColorSpace::DisplayP3()),
                       96, 0);
        });
    ASSERT_NE(texture, nullptr);
    auto pixmap = texture->ReadPixels();
    ASSERT_NE(pixmap, nullptr);
    skity::Bitmap bitmap(pixmap);
    // Interior of the blurred mask keeps alpha 1, so the shadow pixel is
    // the resolved color itself; 16px away from every mask edge.
    auto shadow = bitmap.GetPixel(80, 32);
    auto image = bitmap.GetPixel(112, 32);
    ExpectRGBA(shadow, image, 2);
    EXPECT_LE(ColorGetR(shadow), ColorGetA(shadow));
  }
}

// A legacy sRGB shadow on a wide-gamut target must keep riding the blend
// resolve: same pixels as an undeclared (sRGB) image on the same target.
TEST(ColorSpaceDropShadowGPU, LegacyShadowRidesIntoP3Target) {
  ASSERT_NE(skity::testing::GoldenTestEnv::GetInstance(), nullptr);
  ScopedTarget target(skity::ColorSpace::DisplayP3());
  auto texture = target.env()->RenderToTexture(128, 64, [&](skity::Canvas* c) {
    c->Clear(skity::Color_TRANSPARENT);
    skity::Paint paint;
    paint.SetColor(skity::Color_WHITE);
    paint.SetImageFilter(skity::ImageFilters::DropShadow(
        64, 0, 3, 3, skity::Color_RED, nullptr));
    c->DrawRect(skity::Rect::MakeWH(32, 64), paint);
    c->DrawImage(SolidSpaceImage(skity::Color4f{1.f, 0.f, 0.f, 1.f}, nullptr),
                 96, 0);
  });
  ASSERT_NE(texture, nullptr);
  auto pixmap = texture->ReadPixels();
  ASSERT_NE(pixmap, nullptr);
  skity::Bitmap bitmap(pixmap);
  auto shadow = bitmap.GetPixel(80, 32);
  auto image = bitmap.GetPixel(112, 32);
  ExpectRGBA(shadow, image, 2);
}

// Serialize a P3-declared drop shadow, read it back through the real
// reader, and replay both on a P3 target: the wide-gamut shadow color
// must survive the trip (the serialized form carries the blend filter's
// sRGB-converted extended-range floats).
TEST(ColorSpaceDropShadowIO, RoundTripPreservesDeclaredShadowOnP3Target) {
  ASSERT_NE(skity::testing::GoldenTestEnv::GetInstance(), nullptr);
  const skity::Color4f p3_red{1.f, 0.f, 0.f, 1.f};
  auto filter = skity::ImageFilters::DropShadow(
      32, 0, 3, 3, p3_red, nullptr,
      skity::DropShadowOptions{skity::ColorSpace::DisplayP3()});
  ASSERT_NE(filter, nullptr);
  // Nested flattenables need a factory registry on both sides; this mirrors
  // how the picture serializer wires its buffers.
  skity::FactorySet factory_set;
  skity::BinaryWriteBuffer writer;
  writer.SetFactorySet(&factory_set);
  filter->FlattenToBuffer(writer);
  auto data = Bytes(writer);
  skity::ReadBuffer reader(data.data(), data.size());
  reader.SetFactorySet(&factory_set);
  auto restored = std::static_pointer_cast<skity::ImageFilter>(
      skity::ReadImageFilterFromMemory("SkMatrixTransformImageFilter", reader));
  ASSERT_TRUE(reader.IsValid());
  ASSERT_NE(restored, nullptr);

  ScopedTarget target(skity::ColorSpace::DisplayP3());
  auto texture = target.env()->RenderToTexture(128, 64, [&](skity::Canvas* c) {
    c->Clear(skity::Color_TRANSPARENT);
    skity::Paint paint;
    paint.SetColor(skity::Color_WHITE);
    paint.SetImageFilter(filter);
    c->DrawRect(skity::Rect::MakeWH(32, 64), paint);
    paint.SetImageFilter(restored);
    c->DrawRect(skity::Rect::MakeXYWH(64, 0, 32, 64), paint);
  });
  ASSERT_NE(texture, nullptr);
  auto pixmap = texture->ReadPixels();
  ASSERT_NE(pixmap, nullptr);
  skity::Bitmap bitmap(pixmap);
  auto direct = bitmap.GetPixel(48, 32);
  auto round_trip = bitmap.GetPixel(112, 32);
  // P3 red declared and replayed on a P3 target stays pure red.
  EXPECT_EQ(direct, skity::Color_RED);
  ExpectRGBA(round_trip, direct, 2);
}

// The undeclared-space overloads must produce byte-identical payloads:
// the wire format for legacy callers does not move with this change.
TEST(ColorSpaceDropShadowIO, FlattenWireUnchangedForLegacyColor) {
  auto legacy =
      skity::ImageFilters::DropShadow(10, 4, 3, 3, skity::Color_RED, nullptr);
  auto declared = skity::ImageFilters::DropShadow(
      10, 4, 3, 3, skity::Color4f{1.f, 0.f, 0.f, 1.f}, nullptr);
  ASSERT_NE(legacy, nullptr);
  ASSERT_NE(declared, nullptr);
  skity::FactorySet legacy_factories;
  skity::BinaryWriteBuffer legacy_writer;
  legacy_writer.SetFactorySet(&legacy_factories);
  legacy->FlattenToBuffer(legacy_writer);
  auto legacy_bytes = Bytes(legacy_writer);
  skity::FactorySet declared_factories;
  skity::BinaryWriteBuffer declared_writer;
  declared_writer.SetFactorySet(&declared_factories);
  declared->FlattenToBuffer(declared_writer);
  auto declared_bytes = Bytes(declared_writer);
  ASSERT_EQ(legacy_bytes.size(), declared_bytes.size());
  EXPECT_EQ(0, std::memcmp(legacy_bytes.data(), declared_bytes.data(),
                           legacy_bytes.size()));

  // A declared space does change the payload (the blend node carries the
  // converted color), so the equivalence above is not vacuous.
  skity::FactorySet p3_factories;
  skity::BinaryWriteBuffer p3_writer;
  p3_writer.SetFactorySet(&p3_factories);
  auto p3 = skity::ImageFilters::DropShadow(
      10, 4, 3, 3, skity::Color4f{1.f, 0.f, 0.f, 1.f}, nullptr,
      skity::DropShadowOptions{skity::ColorSpace::DisplayP3()});
  p3->FlattenToBuffer(p3_writer);
  auto p3_bytes = Bytes(p3_writer);
  ASSERT_EQ(p3_bytes.size(), legacy_bytes.size());
  EXPECT_NE(0,
            std::memcmp(p3_bytes.data(), legacy_bytes.data(), p3_bytes.size()));
}
