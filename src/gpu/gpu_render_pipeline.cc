// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/gpu_render_pipeline.hpp"

namespace skity {

GPURenderPipeline::GPURenderPipeline(const GPURenderPipelineDescriptor& desc)
    : desc_(desc) {
  auto vs_groups = desc.vertex_function->GetBindGroups();
  auto fs_groups = desc.fragment_function->GetBindGroups();

  if (vs_groups.empty() && fs_groups.empty()) {
    return;
  }

  auto merge_groups = [&](std::vector<wgx::BindGroup>& groups1,
                          std::vector<wgx::BindGroup>& groups2) -> bool {
    for (auto& group : groups1) {
      for (auto& group2 : groups2) {
        if (group2.group == group.group) {
          if (!group.Merge(group2)) {
            return false;
          }
          break;
        }
      }

      bind_groups_.emplace_back(group);
    }

    for (auto& group : groups2) {
      bool found = false;

      for (auto& group1 : bind_groups_) {
        if (group1.group == group.group) {
          found = true;
          break;
        }
      }

      if (!found) {
        bind_groups_.emplace_back(group);
      }
    }

    return true;
  };

  if (!merge_groups(vs_groups, fs_groups)) {
    valid_ = false;
  }
}

bool GPURenderPipeline::IsValid() const { return valid_; }

}  // namespace skity
