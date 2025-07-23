// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "case/recorder/recorder_example.hpp"

#include <skity/recorder/picture_recorder.hpp>
#include <skity/skity.hpp>

#include "case/basic/example.hpp"

namespace skity::example::recorder {

void draw_with_recorder(skity::Canvas* canvas) {
  skity::PictureRecorder recorder;
  recorder.BeginRecording();

  skity::example::basic::draw_canvas(recorder.GetRecordingCanvas());
  std::unique_ptr<skity::DisplayList> display_list = recorder.FinishRecording();
  display_list->Draw(canvas);
}

}  // namespace skity::example::recorder
