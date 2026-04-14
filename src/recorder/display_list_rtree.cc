// Copyright 2013 The Flutter Authors. All rights reserved.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/recorder/display_list_rtree.hpp"

#include <algorithm>

#include "src/recorder/display_list_region.hpp"

namespace skity {

namespace {

constexpr uint32_t kDisplayListRTreeBranchFactor = 8u;

Rect MakeRoundedOutRect(const Rect& rect) {
  Rect rounded = rect;
  rounded.RoundOut();
  return rounded;
}

}  // namespace

DisplayListRTree::DisplayListRTree(
    std::vector<std::pair<Rect, int32_t>>&& spatial_ops)
    : spatial_ops_(std::move(spatial_ops)) {
  BuildRTree();
}

void DisplayListRTree::BuildRTree() {
  spatial_ops_.erase(std::remove_if(spatial_ops_.begin(), spatial_ops_.end(),
                                    [](const auto& item) {
                                      return item.second < 0 ||
                                             item.first.IsEmpty();
                                    }),
                     spatial_ops_.end());

  rtree_nodes_.clear();
  rtree_children_.clear();
  rtree_root_ = 0;
  if (spatial_ops_.empty()) {
    return;
  }

  const auto append_generation =
      [this](const std::vector<uint32_t>& generation,
             bool leaf_generation) -> std::vector<uint32_t> {
    std::vector<uint32_t> parents;
    if (generation.empty()) {
      return parents;
    }

    const uint32_t gen_count = static_cast<uint32_t>(generation.size());
    const uint32_t family_count =
        (gen_count + kDisplayListRTreeBranchFactor - 1u) /
        kDisplayListRTreeBranchFactor;
    parents.reserve(family_count);

    int32_t d = 0;
    uint32_t sibling_index = 0;
    while (sibling_index < gen_count) {
      RTreeNode node;
      node.leaf = leaf_generation;
      node.first_child = static_cast<uint32_t>(rtree_children_.size());

      uint32_t child_count = 0;
      while (sibling_index < gen_count) {
        if ((d += static_cast<int32_t>(family_count)) > 0) {
          d -= static_cast<int32_t>(gen_count);
          if (child_count != 0) {
            break;
          }
        }

        const uint32_t child = generation[sibling_index++];
        if (leaf_generation) {
          node.bounds.Join(spatial_ops_[child].first);
        } else {
          node.bounds.Join(rtree_nodes_[child].bounds);
        }
        rtree_children_.push_back(child);
        ++child_count;
      }

      node.child_count = child_count;
      rtree_nodes_.push_back(node);
      parents.push_back(static_cast<uint32_t>(rtree_nodes_.size() - 1));
    }

    return parents;
  };

  std::vector<uint32_t> generation(spatial_ops_.size());
  for (uint32_t i = 0; i < generation.size(); ++i) {
    generation[i] = i;
  }

  bool leaf_generation = true;
  while (generation.size() > 1u || rtree_nodes_.empty()) {
    generation = append_generation(generation, leaf_generation);
    leaf_generation = false;
  }

  rtree_root_ = generation.front();
}

void DisplayListRTree::SearchRTreeItems(
    uint32_t node_index, const Rect& rect,
    std::vector<uint32_t>* item_indices) const {
  if (item_indices == nullptr || node_index >= rtree_nodes_.size()) {
    return;
  }

  const auto& node = rtree_nodes_[node_index];
  if (!Rect::Intersect(node.bounds, rect)) {
    return;
  }

  for (uint32_t i = 0; i < node.child_count; ++i) {
    const auto child_index = rtree_children_[node.first_child + i];
    if (node.leaf) {
      const auto& item = spatial_ops_[child_index];
      if (Rect::Intersect(item.first, rect)) {
        item_indices->push_back(child_index);
      }
      continue;
    }

    SearchRTreeItems(child_index, rect, item_indices);
  }
}

std::vector<RecordedOpOffset> DisplayListRTree::Search(const Rect& rect) const {
  std::vector<RecordedOpOffset> results;
  if (rect.IsEmpty() || rtree_nodes_.empty()) {
    return results;
  }

  std::vector<uint32_t> item_indices;
  SearchRTreeItems(rtree_root_, rect, &item_indices);
  results.reserve(item_indices.size());
  for (auto item_index : item_indices) {
    results.push_back(RecordedOpOffset::Make(spatial_ops_[item_index].second));
  }
  std::sort(results.begin(), results.end(),
            [](const RecordedOpOffset& lhs, const RecordedOpOffset& rhs) {
              return lhs.GetValue() < rhs.GetValue();
            });
  return results;
}

std::vector<Rect> DisplayListRTree::SearchNonOverlappingDrawnRects(
    const Rect& rect) const {
  std::vector<Rect> results;
  if (rect.IsEmpty() || rtree_nodes_.empty()) {
    return results;
  }

  std::vector<uint32_t> item_indices;
  SearchRTreeItems(rtree_root_, rect, &item_indices);
  if (item_indices.empty()) {
    return results;
  }

  std::vector<Rect> matched_rects;
  matched_rects.reserve(item_indices.size());
  for (auto item_index : item_indices) {
    Rect rounded = MakeRoundedOutRect(spatial_ops_[item_index].first);
    if (!rounded.IsEmpty()) {
      matched_rects.push_back(rounded);
    }
  }

  if (matched_rects.empty()) {
    return results;
  }
  return DisplayListRegion(matched_rects).GetRects(true);
}

}  // namespace skity
