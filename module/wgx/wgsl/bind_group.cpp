// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl_cross.h"

namespace wgx {

const BindGroupEntry* BindGroup::GetEntry(uint32_t binding) const {
  for (auto& entry : entries) {
    if (entry.binding == binding) {
      return &entry;
    }
  }
  return nullptr;
}

BindGroupEntry* BindGroup::GetEntry(uint32_t binding) {
  for (auto& entry : entries) {
    if (entry.binding == binding) {
      return &entry;
    }
  }
  return nullptr;
}

bool BindGroup::Merge(const BindGroup& other) {
  std::vector<BindGroupEntry> append_entries{};

  for (auto& entry : other.entries) {
    auto* existing_entry = GetEntry(entry.binding);
    if (existing_entry) {
      if (existing_entry->type != entry.type ||
          existing_entry->type_definition->name !=
              entry.type_definition->name) {
        return false;
      }

      existing_entry->stage |= entry.stage;
    } else {
      append_entries.push_back(entry);
    }
  }

  if (!append_entries.empty()) {
    entries.insert(entries.end(), append_entries.begin(), append_entries.end());
  }

  return true;
}

}  // namespace wgx
