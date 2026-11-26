// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/golden_test_env.hpp"

#ifdef SKITY_CPU
#include <cstring>
#include <skity/graphic/bitmap.hpp>
#endif

#ifdef SKITY_GOLDEN_GUI

#include <GLFW/glfw3.h>

#endif

namespace skity {
namespace testing {

#ifdef SKITY_CPU
namespace {

// Adapts the golden surface callbacks to a bitmap-backed canvas.
class SoftwareGoldenSurface final : public GPUSurface {
 public:
  SoftwareGoldenSurface(uint32_t width, uint32_t height,
                        const std::shared_ptr<const ColorSpace>& color_space)
      : bitmap_(width, height, AlphaType::kPremul_AlphaType, ColorType::kRGBA) {
    bitmap_.GetPixmap()->SetColorInfo(bitmap_.GetAlphaType(),
                                      bitmap_.GetColorType(), color_space);
  }

  uint32_t GetWidth() const override { return bitmap_.Width(); }
  uint32_t GetHeight() const override { return bitmap_.Height(); }
  float ContentScale() const override { return 1.f; }

  Canvas* LockCanvas(bool clear) override {
    if (clear) {
      std::memset(bitmap_.GetPixelAddr(), 0,
                  bitmap_.RowBytes() * bitmap_.Height());
    }
    canvas_ = Canvas::MakeSoftwareCanvas(&bitmap_);
    return canvas_.get();
  }

  void Flush() override {}

  std::shared_ptr<Pixmap> ReadPixels(const Rect& rect) override {
    if (rect != Rect::MakeWH(GetWidth(), GetHeight())) {
      return nullptr;
    }
    return bitmap_.GetPixmap();
  }

 private:
  Bitmap bitmap_;
  std::unique_ptr<Canvas> canvas_;
};

class GoldenTestEnvSoftware final : public GoldenTestEnv {
 public:
  Backend GetBackend() const override { return Backend::kSoftware; }

  void SetUp() override {}
  void TearDown() override {}

  std::shared_ptr<GoldenTexture> RenderToSurface(
      uint32_t width, uint32_t height,
      const std::function<void(GPUSurface*)>& render) override {
    if (width == 0 || height == 0) {
      return nullptr;
    }

    SoftwareGoldenSurface surface(width, height, GetTargetColorSpace());
    render(&surface);
    return std::make_shared<GoldenTexture>(
        Image::MakeImage(surface.ReadPixels(Rect::MakeWH(width, height))));
  }

 protected:
  std::unique_ptr<GPUContext> CreateGPUContext() override { return nullptr; }
};

}  // namespace
#endif

GoldenTestEnv* CreateGoldenTestEnvMTL();
GoldenTestEnv* CreateGoldenTestEnvGL();

#ifdef SKITY_GOLDEN_VK
GoldenTestEnv* CreateGoldenTestEnvVK();
#endif

GoldenTestEnv* g_golden_test_env = nullptr;

GoldenTestEnv* GoldenTestEnv::CreateInstance(Backend backend) {
  switch (backend) {
    case Backend::kGL:
      g_golden_test_env = CreateGoldenTestEnvGL();
      break;
    case Backend::kVulkan:
#ifdef SKITY_GOLDEN_VK
      g_golden_test_env = CreateGoldenTestEnvVK();
#endif
      break;
    case Backend::kMetal:
      g_golden_test_env = CreateGoldenTestEnvMTL();
      break;
    case Backend::kSoftware:
#ifdef SKITY_CPU
      g_golden_test_env = new GoldenTestEnvSoftware();
      break;
#else
      return nullptr;
#endif
    default:
      return nullptr;
  }

  return g_golden_test_env;
}

GoldenTestEnv* GoldenTestEnv::GetInstance() { return g_golden_test_env; }

void GoldenTestEnv::SetUp() {
#ifdef SKITY_GOLDEN_GUI
  // Init glfw for all testing case
  glfwInit();
  // Metal does not needs GL client api
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#endif

  gpu_context_ = CreateGPUContext();
}

void GoldenTestEnv::TearDown() {
  gpu_context_.reset();

#ifdef SKITY_GOLDEN_GUI

  glfwTerminate();

#endif
}

}  // namespace testing
}  // namespace skity
