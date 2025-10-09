// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <type_traits>

#include "src/base/file.hpp"
#include "src/base/mapping.hpp"
#include "src/base/platform/win/lean_windows.hpp"

namespace skity {

Mapping::Mapping() = default;

Mapping::~Mapping() = default;

static bool IsWritable(
    std::initializer_list<FileMapping::Protection> protection_flags) {
  for (auto protection : protection_flags) {
    if (protection == FileMapping::Protection::kWrite) {
      return true;
    }
  }
  return false;
}

static bool IsExecutable(
    std::initializer_list<FileMapping::Protection> protection_flags) {
  for (auto protection : protection_flags) {
    if (protection == FileMapping::Protection::kExecute) {
      return true;
    }
  }
  return false;
}

FileMapping::FileMapping(const UniqueFD& fd,
                         std::initializer_list<Protection> protections)
    : size_(0), mapping_(nullptr) {
  if (!fd.is_valid()) {
    return;
  }

  const auto mapping_size = ::GetFileSize(fd.get(), nullptr);

  if (mapping_size == INVALID_FILE_SIZE) {
    // FML_DLOG(ERROR) << "Invalid file size. " << GetLastErrorMessage();
    return;
  }

  if (mapping_size == 0) {
    valid_ = true;
    return;
  }

  DWORD protect_flags = 0;
  bool read_only = !IsWritable(protections);

  if (IsExecutable(protections)) {
    protect_flags = PAGE_EXECUTE_READ;
  } else if (read_only) {
    protect_flags = PAGE_READONLY;
  } else {
    protect_flags = PAGE_READWRITE;
  }

  mapping_handle_.reset(::CreateFileMapping(fd.get(),       // hFile
                                            nullptr,        // lpAttributes
                                            protect_flags,  // flProtect
                                            0,              // dwMaximumSizeHigh
                                            0,              // dwMaximumSizeLow
                                            nullptr         // lpName
                                            ));             // NOLINT

  if (!mapping_handle_.is_valid()) {
    return;
  }

  const DWORD desired_access = read_only ? FILE_MAP_READ : FILE_MAP_WRITE;

  auto mapping = reinterpret_cast<uint8_t*>(
      MapViewOfFile(mapping_handle_.get(), desired_access, 0, 0, mapping_size));

  if (mapping == nullptr) {
    // FML_DLOG(ERROR) << "Could not set up file mapping. "
    //                 << GetLastErrorMessage();
    return;
  }

  mapping_ = mapping;
  size_ = mapping_size;
  valid_ = true;
  if (IsWritable(protections)) {
    mutable_mapping_ = mapping_;
  }
}

FileMapping::~FileMapping() {
  if (mapping_ != nullptr) {
    UnmapViewOfFile(mapping_);
  }
}

size_t FileMapping::GetSize() const { return size_; }

const uint8_t* FileMapping::GetMapping() const { return mapping_; }

bool FileMapping::IsDontNeedSafe() const { return mutable_mapping_ == nullptr; }

bool FileMapping::IsValid() const { return valid_; }

}  // namespace skity
