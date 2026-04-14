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
  return BeginRecording(DisplayListBuilder::kMaxCullRect,
                        DisplayListBuildOptions{});
}

void PictureRecorder::BeginRecording(const Rect& bounds) {
  return BeginRecording(bounds, DisplayListBuildOptions{});
}

void PictureRecorder::BeginRecording(const Rect& bounds,
                                     const DisplayListBuildOptions& options) {
  dp_builder_ = std::make_unique<DisplayListBuilder>(bounds, options);
  canvas_->BindDisplayListBuilder(dp_builder_.get());
}

RecordingCanvas* PictureRecorder::GetRecordingCanvas() { return canvas_.get(); }

std::unique_ptr<DisplayList> PictureRecorder::FinishRecording() {
  while (!dp_builder_->save_op_stack_.empty()) {
    const int32_t save_offset = dp_builder_->save_op_stack_.back();
    dp_builder_->save_op_stack_.pop_back();
    dp_builder_->SetSaveRestoreOffset(save_offset,
                                      static_cast<int32_t>(dp_builder_->used_));
  }
  std::unique_ptr<DisplayList> dl = dp_builder_->GetDisplayList();
  dp_builder_.reset(nullptr);
  return dl;
}

bool PictureRecorder::Empty() { return dp_builder_->used_ == 0; }

}  // namespace skity
