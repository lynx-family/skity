// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cstring>
#include <skity/effect/shader.hpp>
#include <vector>

#include "gtest/gtest.h"

namespace {

// Captures the byte array payloads and the first uint32 (the gradient
// flags word), ignoring everything else the gradient flatten path writes.
class CaptureBuffer : public skity::WriteBuffer {
 public:
  std::vector<std::vector<uint8_t>> chunks;
  std::vector<uint32_t> uint32s;

  void WriteByteArray(const uint8_t* data, size_t size) override {
    chunks.emplace_back(data, data + size);
  }
  void WriteBool(bool) override {}
  void WriteFloat(float) override {}
  void WriteFloatArray(float const*, size_t) override {}
  void WriteInt32(int32_t) override {}
  void WriteUint32(uint32_t v) override { uint32s.push_back(v); }
  void WriteColor(skity::Color) override {}
  void WriteColorArray(skity::Color const*, size_t) override {}
  void WriteColor4f(const skity::Color4f&) override {}
  void WriteColor4fArray(const skity::Color4f*, size_t) override {}
  void WritePoint(const skity::Vec2&) override {}
  void WritePointArray(const skity::Vec2*, size_t) override {}
  void WriteSampling(const skity::SamplingOptions&) override {}
  void WriteMatrix(const skity::Matrix&) override {}
  void WriteRect(const skity::Rect&) override {}
  void WriteImage(const skity::Image*) override {}
  void WriteTypeface(const std::shared_ptr<skity::Typeface>&) override {}
  void WriteFlattenable(const skity::Flattenable*) override {}
};

constexpr uint32_t kHasColorSpaceFlag = 0x20000000u;

}  // namespace

TEST(GradientColorSpace, FactoryDeclaresStopColorSpace) {
  const skity::Point pts[2] = {{0.f, 0.f, 0.f, 1.f}, {64.f, 0.f, 0.f, 1.f}};
  const skity::Vec4 stops[2] = {{1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};

  // The default carries no declaration: null means undeclared (sRGB).
  auto plain = skity::Shader::MakeLinear(pts, stops, nullptr, 2);
  ASSERT_NE(plain, nullptr);
  skity::Shader::GradientInfo info{};
  plain->AsGradient(&info);
  EXPECT_EQ(info.color_space, nullptr);

  auto p3 = skity::Shader::MakeLinear(
      pts, stops, nullptr, 2,
      {skity::TileMode::kClamp, 0, skity::ColorSpace::DisplayP3()});
  ASSERT_NE(p3, nullptr);
  info = {};
  p3->AsGradient(&info);
  EXPECT_EQ(info.color_space, skity::ColorSpace::DisplayP3());
  // The stop values themselves stay untouched until draw time.
  ASSERT_EQ(info.colors.size(), 2u);
  EXPECT_EQ(info.colors[0].r, 1.f);
  EXPECT_EQ(info.colors[1].b, 1.f);
}

TEST(GradientColorSpace, EveryGradientFactoryCarriesDeclaredSpace) {
  const skity::Vec4 stops[2] = {{1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};
  const skity::Point center{32.f, 32.f, 0.f, 1.f};
  const skity::Point pts[2] = {{0.f, 0.f, 0.f, 1.f}, {64.f, 0.f, 0.f, 1.f}};

  const skity::GradientOptions p3_options{skity::TileMode::kClamp, 0,
                                          skity::ColorSpace::DisplayP3()};
  auto radial =
      skity::Shader::MakeRadial(center, 32.f, stops, nullptr, 2, p3_options);
  auto sweep = skity::Shader::MakeSweep(32.f, 32.f, 0.f, 90.f, stops, nullptr,
                                        2, p3_options);
  auto conical = skity::Shader::MakeTwoPointConical(
      pts[0], 1.f, pts[1], 32.f, stops, nullptr, 2, p3_options);

  for (auto* shader : {&radial, &sweep, &conical}) {
    ASSERT_NE(*shader, nullptr);
    skity::Shader::GradientInfo info{};
    (*shader)->AsGradient(&info);
    EXPECT_EQ(info.color_space, skity::ColorSpace::DisplayP3());
  }
}

TEST(GradientColorSpace, FlattenWritesSpaceChunkOnlyWhenDeclared) {
  const skity::Point pts[2] = {{0.f, 0.f, 0.f, 1.f}, {64.f, 0.f, 0.f, 1.f}};
  const skity::Vec4 stops[2] = {{1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};

  // Undeclared keeps the legacy wire: no color space flag, no byte chunk.
  auto plain = skity::Shader::MakeLinear(pts, stops, nullptr, 2);
  ASSERT_NE(plain, nullptr);
  CaptureBuffer plain_buffer;
  plain->FlattenToBuffer(plain_buffer);
  EXPECT_TRUE(plain_buffer.chunks.empty());
  ASSERT_FALSE(plain_buffer.uint32s.empty());
  EXPECT_EQ(plain_buffer.uint32s[0] & kHasColorSpaceFlag, 0u);

  // Declared sets the flag and stores the Skia v1 color space block: a
  // 4-byte version header then the transfer function (7 floats) and the
  // gamut matrix (9 floats).
  auto p3 = skity::Shader::MakeLinear(
      pts, stops, nullptr, 2,
      {skity::TileMode::kClamp, 0, skity::ColorSpace::DisplayP3()});
  ASSERT_NE(p3, nullptr);
  CaptureBuffer p3_buffer;
  p3->FlattenToBuffer(p3_buffer);
  ASSERT_EQ(p3_buffer.chunks.size(), 1u);
  EXPECT_EQ(p3_buffer.uint32s[0] & kHasColorSpaceFlag, kHasColorSpaceFlag);

  const auto& chunk = p3_buffer.chunks[0];
  ASSERT_EQ(chunk.size(), 4 + sizeof(float) * 16);
  // Version header: version 1 plus three reserved bytes.
  EXPECT_EQ(chunk[0], 1);
  EXPECT_EQ(chunk[1], 0);
  EXPECT_EQ(chunk[2], 0);
  EXPECT_EQ(chunk[3], 0);
  auto floats = reinterpret_cast<const float*>(chunk.data() + 4);
  const auto& space = *skity::ColorSpace::DisplayP3();
  EXPECT_EQ(std::memcmp(floats, space.transfer_fn, sizeof(float) * 7), 0);
  EXPECT_EQ(std::memcmp(floats + 7, space.gamut_xyz, sizeof(float) * 9), 0);
}

TEST(GradientColorSpace, NumericSRGBDeclarationKeepsLegacyWire) {
  // A parametric space numerically equal to sRGB serializes like an
  // undeclared one: the flag only carries spaces the reader cannot
  // reconstruct to the sRGB default.
  const float srgb_tf[7] = {
      2.4f,
      static_cast<float>(1 / 1.055),
      static_cast<float>(0.055 / 1.055),
      static_cast<float>(1 / 12.92),
      0.04045f,
      0.f,
      0.f,
  };
  auto numeric_srgb =
      skity::ColorSpace::MakeRGB(srgb_tf, skity::ColorSpace::SRGB()->gamut_xyz);
  ASSERT_NE(numeric_srgb, nullptr);

  const skity::Point pts[2] = {{0.f, 0.f, 0.f, 1.f}, {64.f, 0.f, 0.f, 1.f}};
  const skity::Vec4 stops[2] = {{1.f, 0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}};
  auto shader = skity::Shader::MakeLinear(
      pts, stops, nullptr, 2, {skity::TileMode::kClamp, 0, numeric_srgb});
  ASSERT_NE(shader, nullptr);
  CaptureBuffer buffer;
  shader->FlattenToBuffer(buffer);
  EXPECT_TRUE(buffer.chunks.empty());
}
