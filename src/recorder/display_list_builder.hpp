// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RECORDER_DISPLAY_LIST_BUILDER_HPP
#define SRC_RECORDER_DISPLAY_LIST_BUILDER_HPP

#include <memory>
#include <skity/recorder/display_list.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <utility>
#include <vector>

#include "src/logging.hpp"
#include "src/recorder/display_list_rtree.hpp"
#include "src/recorder/recorded_op.hpp"

namespace skity {

struct DisplayListBuilder {
  static constexpr Rect kMaxCullRect = Rect::MakeLTRB(-1E9F, -1E9F, 1E9F, 1E9F);

  explicit DisplayListBuilder(
      const Rect& cull_rect = kMaxCullRect,
      const DisplayListBuildOptions& options = DisplayListBuildOptions{})
      : bounds_(Rect::MakeEmpty()),
        cull_rect_(cull_rect),
        build_rtree_(options.build_rtree) {}

  DisplayListStorage storage_;
  size_t used_ = 0;
  size_t allocated_ = 0;
  Rect bounds_;
  Rect cull_rect_;
  int32_t last_op_offset_ = -1;

  std::unique_ptr<DisplayList> GetDisplayList() {
    auto display_list = std::make_unique<DisplayList>(
        std::move(storage_), used_, render_op_count_, bounds_, properties_);
    if (build_rtree_) {
      display_list->SetRTree(
          std::make_unique<DisplayListRTree>(std::move(spatial_ops_)));
    }
    return display_list;
  }

  void SetSaveRestoreOffset(int32_t save_offset, int32_t restore_offset) {
    auto* op = reinterpret_cast<RecordedOp*>(storage_.get() + save_offset);
    switch (op->type) {
      case RecordedOpType::kSave:
        static_cast<SaveOp*>(op)->restore_offset = restore_offset;
        return;
      case RecordedOpType::kSaveLayer:
        static_cast<SaveLayerOp*>(op)->restore_offset = restore_offset;
        return;
      default:
        DEBUG_CHECK(false);
        return;
    }
  }

  uint32_t render_op_count_ = 0u;
  uint32_t properties_ = 0u;
  bool build_rtree_ = false;
  std::vector<std::pair<Rect, int32_t>> spatial_ops_;
  std::vector<int32_t> save_op_stack_;
};

}  // namespace skity

#endif  // SRC_RECORDER_DISPLAY_LIST_BUILDER_HPP
