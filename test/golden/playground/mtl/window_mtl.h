// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <skity/gpu/gpu_context.hpp>
#include <skity/skity.hpp>

#include "playground/window.hpp"

namespace skity {
namespace testing {

class WindowMTL : public Window {
 public:
  WindowMTL(int32_t w, int32_t h, std::string title);
  ~WindowMTL() override = default;

 protected:
  GLFWwindow* InitWindow() override;

  bool OnShow(bool passed, std::shared_ptr<GoldenTexture> source,
              std::shared_ptr<Pixmap> target) override;

  void OnRender() override;

  void OnCloseWindow() override;

 private:
  bool InitTextures(bool passed, std::shared_ptr<GoldenTexture> source,
                    std::shared_ptr<Pixmap> target);

  void DrawImageInCenter(skity::Canvas* canvas,
                         const std::shared_ptr<Image>& image, Rect rect);

 private:
  CAMetalLayer* metal_layer_ = nil;

  id<MTLDevice> device_ = nil;
  id<MTLCommandQueue> command_queue_ = nil;

  skity::GPUContext* gpu_context_ = nullptr;

  std::shared_ptr<Image> source_image_ = nullptr;
  std::shared_ptr<Image> target_image_ = nullptr;

  std::shared_ptr<Image> diff_image_ = nullptr;
  std::shared_ptr<Image> isolate_diff_image_ = nullptr;
};

}  // namespace testing
}  // namespace skity
