// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/app.hpp"
#include "perf.hpp"

enum {
  kScreenWidth = 1000,
  kScreenHeight = 600,
};

void load_images(std::vector<std::shared_ptr<skity::Pixmap>>& images);

void render_frame_demo(
    skity::Canvas* canvas, skity::GPUContext* gpu_context,
    std::vector<std::shared_ptr<skity::Pixmap>> const& images,
    skity::Typeface* typeface, skity::Typeface* emoji, float mx, float my,
    float width, float height, float t);

class FrameExample : public skity::example::WindowClient {
 public:
  FrameExample()
      : fpsGraph(Perf::GRAPH_RENDER_FPS, "Frame Time"),
        cpuGraph(Perf::GRAPH_RENDER_MS, "CPU Time") {}

  ~FrameExample() override = default;

 protected:
  void OnStart(skity::GPUContext* context) override {
    load_images(images_);
    typeface_ = skity::Typeface::MakeFromFile(
        EXAMPLE_IMAGE_ROOT "/RobotoMonoNerdFont-Regular.ttf");

    emoji_typeface_ =
        skity::Typeface::MakeFromFile(EXAMPLE_IMAGE_ROOT "/NotoColorEmoji.ttf");

    time_ = prev_time_ = glfwGetTime();
  }

  void OnDraw(skity::GPUContext* context, skity::Canvas* canvas) override {
    double mx, my;
    GetWindow()->GetCursorPos(mx, my);

    time_ = glfwGetTime();

    double dt = time_ - prev_time_;
    prev_time_ = time_;

    canvas->DrawColor(skity::Color4f{0.3f, 0.3f, 0.32f, 1.f},
                      skity::BlendMode::kSrc);

    render_frame_demo(canvas, context, images_, typeface_, emoji_typeface_, mx,
                      my, kScreenWidth, kScreenHeight, time_);
    cpu_time_ = glfwGetTime() - time_;
    fpsGraph.RenderGraph(canvas, 5, 5);
    cpuGraph.RenderGraph(canvas, 5 + 200 + 5, 5);

    fpsGraph.UpdateGraph(dt);
    cpuGraph.UpdateGraph(cpu_time_);
  }

  void OnTerminate() override { images_.clear(); }

 private:
  std::vector<std::shared_ptr<skity::Pixmap>> images_ = {};
  skity::Typeface* typeface_ = {};
  skity::Typeface* emoji_typeface_ = {};
  double time_ = 0;
  double prev_time_ = 0;
  double cpu_time_ = 0;
  Perf fpsGraph;
  Perf cpuGraph;
};

int main(int argc, const char** argv) {
  FrameExample example;
  return skity::example::StartExampleApp(argc, argv, example, kScreenWidth,
                                         kScreenHeight, "Frame Example");
}
