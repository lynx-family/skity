// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/base/mapping.hpp"

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>

namespace skity {

// FileMapping

uint8_t* FileMapping::GetMutableMapping() { return mutable_mapping_; }

std::unique_ptr<FileMapping> FileMapping::CreateReadOnly(
    const std::string& path) {
  return CreateReadOnly(OpenFile(path.c_str(), false, FilePermission::kRead),
                        "");
}

std::unique_ptr<FileMapping> FileMapping::CreateReadOnly(
    const UniqueFD& base_fd, const std::string& sub_path) {
  if (!sub_path.empty()) {
    return CreateReadOnly(
        OpenFile(base_fd, sub_path.c_str(), false, FilePermission::kRead), "");
  }

  auto mapping = std::make_unique<FileMapping>(
      base_fd, std::initializer_list<Protection>{Protection::kRead});

  if (!mapping->IsValid()) {
    return nullptr;
  }

  return mapping;
}

std::unique_ptr<FileMapping> FileMapping::CreateReadExecute(
    const std::string& path) {
  return CreateReadExecute(
      OpenFile(path.c_str(), false, FilePermission::kRead));
}

std::unique_ptr<FileMapping> FileMapping::CreateReadExecute(
    const UniqueFD& base_fd, const std::string& sub_path) {
  if (!sub_path.empty()) {
    return CreateReadExecute(
        OpenFile(base_fd, sub_path.c_str(), false, FilePermission::kRead), "");
  }

  auto mapping = std::make_unique<FileMapping>(
      base_fd, std::initializer_list<Protection>{Protection::kRead,
                                                 Protection::kExecute});

  if (!mapping->IsValid()) {
    return nullptr;
  }

  return mapping;
}

}  // namespace skity
