// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RECORDER_DISPLAY_LIST_BUILDER_HPP
#define SRC_RECORDER_DISPLAY_LIST_BUILDER_HPP

#include <skity/recorder/display_list.hpp>

namespace skity {

struct DisplayListBuilder {
  static constexpr Rect kMaxCullRect = Rect::MakeLTRB(-1E9F, -1E9F, 1E9F, 1E9F);

  explicit DisplayListBuilder(const Rect& cull_rect = kMaxCullRect)
      : bounds_(Rect::MakeEmpty()), cull_rect_(cull_rect) {}

  DisplayListStorage storage_;
  size_t used_ = 0;
  size_t allocated_ = 0;
  Rect bounds_;
  Rect cull_rect_;
  int32_t last_op_offset_ = -1;

  std::unique_ptr<DisplayList> GetDisplayList() {
    return std::make_unique<DisplayList>(std::move(storage_), used_,
                                         render_op_count_, bounds_);
  }

  uint32_t render_op_count_ = 0u;
};

}  // namespace skity

#endif  // SRC_RECORDER_DISPLAY_LIST_BUILDER_HPP
