// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/recorder/picture_recorder.hpp>

#include "src/recorder/display_list_builder.hpp"

namespace skity {

PictureRecorder::PictureRecorder()
    : dp_builder_(), canvas_(std::make_unique<RecordingCanvas>()) {}
PictureRecorder::~PictureRecorder() {}

void PictureRecorder::BeginRecording() {
  return BeginRecording(DisplayListBuilder::kMaxCullRect);
}

void PictureRecorder::BeginRecording(const Rect& bounds) {
  dp_builder_ = std::make_unique<DisplayListBuilder>(bounds);
  canvas_->BindDisplayListBuilder(dp_builder_.get());
}

RecordingCanvas* PictureRecorder::GetRecordingCanvas() { return canvas_.get(); }

std::unique_ptr<DisplayList> PictureRecorder::FinishRecording() {
  std::unique_ptr<DisplayList> dl = dp_builder_->GetDisplayList();
  dp_builder_.reset(nullptr);
  return dl;
}

bool PictureRecorder::Empty() { return dp_builder_->used_ == 0; }

}  // namespace skity
