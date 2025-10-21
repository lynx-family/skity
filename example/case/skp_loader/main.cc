// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <iostream>
#include <skity/skity.hpp>

// serialize module
#include <skity/io/picture.hpp>
#include <skity/io/stream.hpp>

#include "common/app.hpp"

using namespace skity;

class SKPLoaderExample : public example::WindowClient {
 public:
  SKPLoaderExample(const char* path) : skp_file_path_(path) {}

  ~SKPLoaderExample() override = default;

 protected:
  void OnStart(GPUContext* context) override {
    auto stream = ReadStream::CreateFromFile(skp_file_path_);

    if (stream == nullptr) {
      return;
    }

    auto picture = Picture::MakeFromStream(*stream);

    if (picture == nullptr) {
      return;
    }

    PictureRecorder recorder{};

    recorder.BeginRecording();

    auto canvas = recorder.GetRecordingCanvas();

    picture->PlayBack(canvas);

    dl_ = recorder.FinishRecording();

    auto bounds = picture->GetCullRect();

    auto origin = std::max(bounds.Width(), bounds.Height());

    if (origin > 1200) {
      scale_x_ = scale_y_ = 1200 / origin;
    }

    cull_rect_ = bounds;
  }

  void OnDraw(GPUContext* context, Canvas* canvas) override {
    if (dl_ == nullptr) {
      return;
    }

    canvas->DrawColor(Color_WHITE);

    auto count = canvas->Save();

    canvas->Scale(scale_x_, scale_y_);

    canvas->ClipRect(cull_rect_);

    dl_->Draw(canvas);

    canvas->RestoreToCount(count);
  }

  void OnTerminate() override { dl_.reset(); }

 private:
  const char* skp_file_path_;
  std::unique_ptr<DisplayList> dl_ = {};

  float scale_x_ = 1.f;
  float scale_y_ = 1.f;

  Rect cull_rect_ = {};
};

int main(int argc, const char** argv) {
  if (argc < 3) {
    std::cerr << "usage: skp-loader <backend> <path to skp file>" << std::endl;

    return 1;
  }

  SKPLoaderExample skp_loader(argv[2]);

  return example::StartExampleApp(argc, argv, skp_loader, 1200, 1200,
                                  "SKP Loader Example");
}
