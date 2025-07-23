// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_RECORDER_PICTURE_RECORDER_HPP
#define INCLUDE_SKITY_RECORDER_PICTURE_RECORDER_HPP

#include <skity/macros.hpp>
#include <skity/recorder/display_list.hpp>
#include <skity/recorder/recording_canvas.hpp>

namespace skity {

class RecordingCanvas;

class SKITY_API PictureRecorder {
 public:
  PictureRecorder();
  ~PictureRecorder();

  RecordingCanvas* GetRecordingCanvas();
  bool Empty();
  void BeginRecording();
  void BeginRecording(const Rect& bounds);
  std::unique_ptr<DisplayList> FinishRecording();

 private:
  std::unique_ptr<DisplayListBuilder> dp_builder_;
  std::unique_ptr<RecordingCanvas> canvas_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_RECORDER_PICTURE_RECORDER_HPP
