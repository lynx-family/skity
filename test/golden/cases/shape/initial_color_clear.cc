// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/skity.hpp>

#include "common/golden_test_env.hpp"

TEST(InitialColorClearGolden, RetainedFrames) {
  using namespace skity;
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  auto old_samples = env->GetSampleCount();
  auto old_mode = env->GetGLSurfaceMode();
  auto old_stencil = env->GetGLHasStencilAttachment();
  env->SetSampleCount(1);
  env->SetGLSurfaceMode(GLSurfaceMode::kDirect);
  env->SetGLHasStencilAttachment(true);

  auto texture = env->RenderToSurface(8, 8, [](GPUSurface* surface) {
    auto* canvas = surface->LockCanvas();
    canvas->Clear(Color_GREEN);
    Paint paint;
    paint.SetColor(Color_BLUE);
    canvas->DrawRect(Rect::MakeWH(4, 8), paint);
    canvas->Flush();
    surface->Flush();

    canvas = surface->LockCanvas(false);
    canvas->Clear(Color4f{1, 0, 0, 128.f / 255});
    paint.SetColor(ColorSetARGB(128, 0, 0, 255));
    canvas->DrawRect(Rect::MakeWH(4, 8), paint);
    canvas->Flush();
    surface->Flush();

    canvas = surface->LockCanvas(false);
    canvas->Save();
    canvas->ClipRect(Rect::MakeWH(2, 8));
    canvas->Clear(Color_WHITE);
    canvas->Restore();
    canvas->Flush();
    surface->Flush();
  });
  env->SetSampleCount(old_samples);
  env->SetGLSurfaceMode(old_mode);
  env->SetGLHasStencilAttachment(old_stencil);

  ASSERT_NE(texture, nullptr);
  auto pixels = texture->ReadPixels();
  ASSERT_NE(pixels, nullptr);
  Bitmap bitmap(std::move(pixels));
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      SCOPED_TRACE(::testing::Message() << x << ", " << y);
      Color expected = ColorSetARGB(128, 128, 0, 0);
      if (x < 2) {
        expected = Color_WHITE;
      } else if (x < 4) {
        expected = ColorSetARGB(192, 64, 0, 128);
      }
      EXPECT_EQ(bitmap.GetPixel(x, y), expected);
    }
  }
}

TEST(InitialColorClearGolden, MultisampleClearOnlyResolve) {
  using namespace skity;
  auto* env = skity::testing::GoldenTestEnv::GetInstance();
  if (env->GetBackend() == skity::testing::Backend::kGL) {
    GTEST_SKIP() << "GL Direct requires a single-sample surface";
  }

  auto* context = env->GetGPUContext();
  GPURenderTargetDescriptor desc{};
  desc.width = desc.height = 8;
  desc.sample_count = 4;
  auto target = context->CreateRenderTarget(desc);
  ASSERT_NE(target, nullptr);
  target->GetCanvas()->Clear(Color4f{1, 0, 0, 128.f / 255});
  auto image = context->MakeSnapshot(std::move(target));
  ASSERT_NE(image, nullptr);
  auto pixels = image->ReadPixels(context);
  ASSERT_NE(pixels, nullptr);
  Bitmap bitmap(std::move(pixels));
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      EXPECT_EQ(bitmap.GetPixel(x, y), ColorSetARGB(128, 128, 0, 0));
    }
  }
}
