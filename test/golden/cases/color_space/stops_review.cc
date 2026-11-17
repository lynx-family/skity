// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/color_space.hpp>
#include <skity/graphic/image.hpp>
#include <vector>

#include "common/golden_test_env.hpp"
#include "module/io/src/io/memory_read.hpp"
#include "module/io/src/io/memory_writer.hpp"
#include "module/io/src/stream/stream.hpp"

namespace skity {
std::shared_ptr<Flattenable> ReadShaderFromMemory(const std::string& factory,
                                                ReadBuffer& buffer);
std::shared_ptr<Flattenable> ReadColorFilterFromMemory(
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

std::shared_ptr<skity::Image> HalfAlphaP3Red() {
  auto pixmap = std::make_shared<skity::Pixmap>(32, 64);
  skity::Bitmap bitmap(pixmap, false);
  for (uint32_t y = 0; y < 64; ++y) {
    for (uint32_t x = 0; x < 32; ++x) {
      bitmap.SetPixel(x, y, skity::ColorSetARGB(128, 255, 0, 0));
    }
  }
  pixmap->SetColorInfo(skity::kUnpremul_AlphaType, skity::ColorType::kRGBA,
                       skity::ColorSpace::DisplayP3());
  return skity::Image::MakeImage(pixmap);
}

void ExpectRGBA(skity::Color actual, skity::Color expected, int tolerance) {
  EXPECT_NEAR(ColorGetR(actual), ColorGetR(expected), tolerance);
  EXPECT_NEAR(ColorGetG(actual), ColorGetG(expected), tolerance);
  EXPECT_NEAR(ColorGetB(actual), ColorGetB(expected), tolerance);
  EXPECT_NEAR(ColorGetA(actual), ColorGetA(expected), tolerance);
}

}  // namespace

TEST(ColorSpaceStopsReviewIO, ReadsSkiaV1GradientColorSpace) {
  // SkColorSpace::writeToMemory: 4-byte version header, 7 TF floats,
  // 9 matrix floats. This is a normal Skia payload, not malformed input.
  auto p3 = skity::ColorSpace::DisplayP3();
  std::array<uint8_t, 68> space{};
  space[0] = 1;
  std::memcpy(space.data() + 4, p3->transfer_fn, sizeof(float) * 7);
  std::memcpy(space.data() + 32, p3->gamut_xyz, sizeof(float) * 9);

  skity::BinaryWriteBuffer writer;
  writer.WriteUint32(0x20000000u);
  const skity::Color4f colors[] = {{1.f, 0.f, 0.f, 1.f},
                                  {0.f, 0.f, 1.f, 1.f}};
  writer.WriteColor4fArray(colors, 2);
  writer.WriteByteArray(space.data(), space.size());
  writer.WritePoint({0.f, 0.f});
  writer.WritePoint({64.f, 0.f});
  auto data = Bytes(writer);
  skity::ReadBuffer reader(data.data(), data.size());
  auto shader = skity::ReadShaderFromMemory("SkLinearGradient", reader);
  EXPECT_TRUE(reader.IsValid());
  ASSERT_NE(shader, nullptr);
}

TEST(ColorSpaceStopsReviewIO, WritesSkiaV1GradientColorSpace) {
  const skity::Point points[] = {{0.f, 0.f, 0.f, 1.f},
                                 {64.f, 0.f, 0.f, 1.f}};
  const skity::Color4f colors[] = {{1.f, 0.f, 0.f, 1.f},
                                  {0.f, 0.f, 1.f, 1.f}};
  auto shader = skity::Shader::MakeLinear(
      points, colors, nullptr, 2,
      {skity::TileMode::kClamp, 0, skity::ColorSpace::DisplayP3()});
  ASSERT_NE(shader, nullptr);
  skity::BinaryWriteBuffer writer;
  shader->FlattenToBuffer(writer);
  auto data = Bytes(writer);
  skity::ReadBuffer reader(data.data(), data.size());
  EXPECT_NE(reader.ReadU32() & 0x20000000u, 0u);
  std::array<skity::Color4f, 2> read_colors;
  ASSERT_TRUE(reader.ReadArrayN<skity::Color4f>(read_colors.data(), 2));
  auto space = reader.ReadByteArrayAsData();
  ASSERT_NE(space, nullptr);
  EXPECT_EQ(space->Size(), 68u);
  ASSERT_GE(space->Size(), 4u);
  const auto* header = static_cast<const uint8_t*>(space->RawData());
  EXPECT_EQ(header[0], 1u);
}

TEST(ColorSpaceStopsReviewGPU, GradientClampsBeforePremultiplying) {
  ASSERT_NE(skity::testing::GoldenTestEnv::GetInstance(), nullptr);
  ScopedTarget target(nullptr);
  for (int flags : {0, 1}) {
    SCOPED_TRACE(flags);
    auto texture = target.env()->RenderToTexture(64, 64, [&](skity::Canvas* c) {
      c->Clear(skity::Color_TRANSPARENT);
      const skity::Point points[] = {{0.f, 0.f, 0.f, 1.f},
                                     {32.f, 0.f, 0.f, 1.f}};
      const skity::Color4f stops[] = {{1.f, 0.f, 0.f, 0.5f},
                                     {1.f, 0.f, 0.f, 0.5f}};
      skity::Paint paint;
      paint.SetShader(skity::Shader::MakeLinear(
          points, stops, nullptr, 2,
          {skity::TileMode::kClamp, flags, skity::ColorSpace::DisplayP3()}));
      c->DrawRect(skity::Rect::MakeWH(32, 64), paint);
      c->DrawImage(HalfAlphaP3Red(), 32, 0);
    });
    ASSERT_NE(texture, nullptr);
    auto pixmap = texture->ReadPixels();
    ASSERT_NE(pixmap, nullptr);
    skity::Bitmap bitmap(pixmap);
    auto gradient = bitmap.GetPixel(16, 32);
    auto image = bitmap.GetPixel(48, 32);
    EXPECT_NEAR(ColorGetR(image), 128, 1);
    EXPECT_NEAR(ColorGetA(image), 128, 1);
    ExpectRGBA(gradient, image, 2);
    EXPECT_LE(ColorGetR(gradient), ColorGetA(gradient));
  }
}
