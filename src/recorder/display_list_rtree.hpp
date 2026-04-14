// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RECORDER_DISPLAY_LIST_RTREE_HPP
#define SRC_RECORDER_DISPLAY_LIST_RTREE_HPP

#include <skity/recorder/display_list.hpp>
#include <utility>
#include <vector>

namespace skity {

class DisplayListRTree {
 public:
  explicit DisplayListRTree(
      std::vector<std::pair<Rect, int32_t>>&& spatial_ops);

  std::vector<RecordedOpOffset> Search(const Rect& rect) const;
  std::vector<Rect> SearchNonOverlappingDrawnRects(const Rect& rect) const;

 private:
  struct RTreeNode {
    Rect bounds = Rect::MakeEmpty();
    uint32_t first_child = 0;
    uint32_t child_count = 0;
    bool leaf = false;
  };

  void BuildRTree();
  void SearchRTreeItems(uint32_t node_index, const Rect& rect,
                        std::vector<uint32_t>* item_indices) const;

  std::vector<std::pair<Rect, int32_t>> spatial_ops_;
  std::vector<RTreeNode> rtree_nodes_;
  std::vector<uint32_t> rtree_children_;
  uint32_t rtree_root_ = 0;
};

}  // namespace skity

#endif  // SRC_RECORDER_DISPLAY_LIST_RTREE_HPP
