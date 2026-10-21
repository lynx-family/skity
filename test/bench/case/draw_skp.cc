// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/case/draw_skp.hpp"

#include <skity/c/skity_picture.h>
#include <skity/c/skity_stream.h>

#include <cassert>

#include "handle.hpp"

namespace skity {

DrawSKPBenchmark::DrawSKPBenchmark(std::string name, const char* skp_file_path,
                                   uint32_t width, uint32_t height,
                                   Matrix matrix)
    : name_(name), width_(width), height_(height) {
  // Picture / stream go through the module's C ABI; the recorder stays on
  // the core legacy C++ surface the bench harness uses.
  skity_read_stream stream = skity_read_stream_create_from_file(skp_file_path);
  assert(stream != nullptr);
  skity_picture picture = skity_picture_create_from_stream(stream);
  assert(picture != nullptr);

  PictureRecorder recorder;
  recorder.BeginRecording(Rect::MakeWH(width, height));
  auto canvas = recorder.GetRecordingCanvas();
  canvas->Concat(matrix);

  std::shared_ptr<Canvas> canvas_view(canvas, [](Canvas*) {});
  skity_canvas canvas_handle = skity::capi::alloc_handle<skity_canvas_s>(
      SKITY_OBJECT_TYPE_CANVAS, 0, std::move(canvas_view));
  skity_picture_play_back(picture, canvas_handle);

  display_list_ = recorder.FinishRecording();

  skity_picture_destroy(picture);
  skity_read_stream_destroy(stream);
}

void DrawSKPBenchmark::OnDraw(Canvas* canvas, int index) {
  display_list_->Draw(canvas);
}

}  // namespace skity
