// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/case/draw_skp.hpp"

#include <cassert>

namespace skity {

DrawSKPBenchmark::DrawSKPBenchmark(std::string name, const char* skp_file_path,
                                   uint32_t width, uint32_t height,
                                   Matrix matrix)
    : name_(name), width_(width), height_(height) {
  auto stream = skity::ReadStream::CreateFromFile(skp_file_path);
  assert(stream != nullptr);
  auto picture = skity::Picture::MakeFromStream(*stream);
  assert(picture != nullptr);

  skity::PictureRecorder recorder;
  recorder.BeginRecording(skity::Rect::MakeWH(width, height));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Concat(matrix);
  picture->PlayBack(canvas);
  display_list_ = recorder.FinishRecording();
}

void DrawSKPBenchmark::OnDraw(Canvas* canvas, int index) {
  display_list_->Draw(canvas);
}

}  // namespace skity
