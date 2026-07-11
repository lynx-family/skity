// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <skity/graphic/image.hpp>
#include <skity/io/pixmap.hpp>

using namespace skity;

namespace {

// A GetPromiseTexture2 callback that never actually produces a texture. This
// keeps the PromiseTextureImage test fully GPU-free: no real Texture or
// GPUContext is ever constructed.
std::shared_ptr<Texture> NullPromiseTexture(PromiseTextureContext,
                                            GPUContext*) {
  return nullptr;
}

void NoopReleaseCallback(ReleaseUserData) {}

}  // namespace

TEST(ImageTest, MakeImageFromCpuPixmap) {
  auto pixmap = std::make_shared<Pixmap>(100, 200);

  auto image = Image::MakeImage(pixmap);

  ASSERT_NE(image, nullptr);
  EXPECT_FALSE(image->IsTextureBackend());
  EXPECT_EQ(image->GetTexture(), nullptr);

  auto image_pixmap = image->GetPixmap();
  ASSERT_NE(image_pixmap, nullptr);
  ASSERT_NE(*image_pixmap, nullptr);
  EXPECT_EQ((*image_pixmap)->Width(), pixmap->Width());
  EXPECT_EQ((*image_pixmap)->Height(), pixmap->Height());

  EXPECT_EQ(image->Width(), pixmap->Width());
  EXPECT_EQ(image->Height(), pixmap->Height());
  EXPECT_EQ(image->GetAlphaType(), pixmap->GetAlphaType());
  EXPECT_EQ(image->GetImageType(), ImageType::kPixmap);
  EXPECT_FALSE(image->IsLazy());
}

TEST(ImageTest, MakeDeferredTextureImage) {
  auto image = Image::MakeDeferredTextureImage(TextureFormat::kRGBA, 64, 128,
                                               AlphaType::kPremul_AlphaType);

  ASSERT_NE(image, nullptr);
  EXPECT_TRUE(image->IsTextureBackend());
  EXPECT_EQ(image->GetPixmap(), nullptr);
  // No texture has been set yet, so GetTexture() must report null.
  EXPECT_EQ(image->GetTexture(), nullptr);

  EXPECT_EQ(image->Width(), 64u);
  EXPECT_EQ(image->Height(), 128u);
  EXPECT_EQ(image->GetAlphaType(), AlphaType::kPremul_AlphaType);
  EXPECT_EQ(image->GetFormat(), TextureFormat::kRGBA);
  EXPECT_EQ(image->GetImageType(), ImageType::kDeferredTexture);
  EXPECT_FALSE(image->IsLazy());
}

TEST(ImageTest, MakePromiseTextureImage2IsLazy) {
  auto image = Image::MakePromiseTextureImage2(
      TextureFormat::kRGBA, 32, 32, AlphaType::kPremul_AlphaType,
      &NullPromiseTexture, &NoopReleaseCallback, nullptr);

  ASSERT_NE(image, nullptr);
  EXPECT_TRUE(image->IsTextureBackend());
  EXPECT_TRUE(image->IsLazy());
  EXPECT_EQ(image->GetImageType(), ImageType::kPromiseTexture);
  EXPECT_EQ(image->GetPixmap(), nullptr);

  // Resolving the promise texture with a null GPUContext should simply
  // forward to the callback (which returns nullptr) without crashing.
  auto texture = image->GetTextureByContext(nullptr);
  EXPECT_EQ(texture, nullptr);
}
