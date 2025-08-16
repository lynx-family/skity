// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/io/pixmap.hpp>

#include <gtest/gtest.h>

using namespace skity;

TEST(PixmapTest, Constructor) {
  Pixmap pixmap;
  ASSERT_EQ(pixmap.Width(), 0);
  ASSERT_EQ(pixmap.Height(), 0);
  ASSERT_EQ(pixmap.RowBytes(), 0);
  ASSERT_EQ(pixmap.Addr(), nullptr);
  ASSERT_EQ(pixmap.GetAlphaType(), AlphaType::kUnknown_AlphaType);
  ASSERT_EQ(pixmap.GetColorType(), ColorType::kUnknown);
}

TEST(PixmapTest, ConstructorWithParams) {
  Pixmap pixmap(100, 200);
  ASSERT_EQ(pixmap.Width(), 100);
  ASSERT_EQ(pixmap.Height(), 200);
  ASSERT_NE(pixmap.Addr(), nullptr);
  ASSERT_EQ(pixmap.GetAlphaType(), AlphaType::kUnpremul_AlphaType);
  ASSERT_EQ(pixmap.GetColorType(), ColorType::kRGBA);
}

TEST(PixmapTest, RowBytes) {
  Pixmap pixmap(100, 200);
  // The default is ColorType::kRGBA, which occupies 4 bytes
  ASSERT_EQ(pixmap.RowBytes(), 100 * 4); 
}

// pay attention to the matching rules
TEST(PixmapTest, SetColorInfoAlphaType) {
  Pixmap pixmap;
  pixmap.SetColorInfo(AlphaType::kUnknown_AlphaType, ColorType::kUnknown);
  ASSERT_EQ(pixmap.GetAlphaType(), skity::AlphaType::kUnknown_AlphaType);
}

TEST(PixmapTest, SetColorInfoColorType) {
  Pixmap pixmap;
  pixmap.SetColorInfo(AlphaType::kUnpremul_AlphaType, ColorType::kA8);
  ASSERT_EQ(pixmap.GetColorType(), skity::ColorType::kA8);
}

TEST(PixmapTest, SetColorInfo) {
  Pixmap pixmap;
  pixmap.SetColorInfo(AlphaType::kPremul_AlphaType, ColorType::kBGRA);
  ASSERT_EQ(pixmap.GetAlphaType(), skity::AlphaType::kPremul_AlphaType);
  ASSERT_EQ(pixmap.GetColorType(), skity::ColorType::kBGRA);
}

TEST(PixmapTest, GetID) {
  Pixmap pixmap;
  uint32_t id1 = pixmap.GetID();
  pixmap.NotifyPixelsChanged();
  uint32_t id2 = pixmap.GetID();
  ASSERT_NE(id1, id2);
}

TEST(PixmapTest, Reset) {
  Pixmap pixmap(50, 50);
  ASSERT_NE(pixmap.Addr(), nullptr);
  pixmap.Reset();
  ASSERT_EQ(pixmap.Width(), 0);
  ASSERT_EQ(pixmap.Height(), 0);
  ASSERT_EQ(pixmap.RowBytes(), 0);
  ASSERT_EQ(pixmap.Addr(), nullptr);
}

TEST(PixmapTest, Addr8AndWritableAddr8) {
  Pixmap pixmap(10, 10);
  uint8_t* addr = pixmap.WritableAddr8(2, 3);
  const uint8_t* caddr = pixmap.Addr8(2, 3);
  ASSERT_EQ(addr, caddr);
  addr[0] = 123;
  ASSERT_EQ(caddr[0], 123);
}

TEST(PixmapTest, Addr16AndWritableAddr16) {
  Pixmap pixmap(10, 10, AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);
  uint16_t* addr = pixmap.WritableAddr16(1, 1);
  const uint16_t* caddr = pixmap.Addr16(1, 1);
  ASSERT_EQ(addr, caddr);
  addr[0] = 0xABCD;
  ASSERT_EQ(caddr[0], 0xABCD);
}

struct TestListener : public Pixmap::PixelsChangeListener {
  bool notified = false;
  void OnPixelsChange(uint32_t) override { notified = true; }
};

TEST(PixmapTest, PixelsChangeListener) {
  Pixmap pixmap(10, 10);
  auto listener = std::make_shared<TestListener>();
  pixmap.AddPixelsChangeListener(listener);
  pixmap.NotifyPixelsChanged();
  ASSERT_TRUE(listener->notified);
}

TEST(PixmapTest, SmallSizeRowBytes) {
  Pixmap pixmap(1, 1, AlphaType::kUnpremul_AlphaType, ColorType::kA8);
  ASSERT_EQ(pixmap.RowBytes(), 1u);
}

TEST(PixmapTest, MultipleNotifyPixelsChanged) {
  Pixmap pixmap(10, 10);
  uint32_t id1 = pixmap.GetID();
  pixmap.NotifyPixelsChanged();
  uint32_t id2 = pixmap.GetID();
  pixmap.NotifyPixelsChanged();
  uint32_t id3 = pixmap.GetID();
  ASSERT_NE(id1, id2);
  ASSERT_NE(id2, id3);
  ASSERT_NE(id1, id3);
}