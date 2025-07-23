// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_MAPPING_HPP
#define SRC_BASE_MAPPING_HPP

#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "src/base/base_macros.hpp"
#include "src/base/file.hpp"
#include "src/base/unique_fd.hpp"

namespace skity {

class Mapping {
 public:
  Mapping();

  virtual ~Mapping();

  virtual size_t GetSize() const = 0;

  virtual const uint8_t* GetMapping() const = 0;

  // Whether calling madvise(DONTNEED) on the mapping is non-destructive.
  // Generally true for file-mapped memory and false for anonymous memory.
  virtual bool IsDontNeedSafe() const = 0;

 private:
  SKITY_DISALLOW_COPY_AND_ASSIGN(Mapping);
};

class FileMapping final : public Mapping {
 public:
  enum class Protection {
    kRead,
    kWrite,
    kExecute,
  };

  explicit FileMapping(const UniqueFD& fd,
                       std::initializer_list<Protection> protection = {
                           Protection::kRead});

  ~FileMapping() override;

  static std::unique_ptr<FileMapping> CreateReadOnly(const std::string& path);

  static std::unique_ptr<FileMapping> CreateReadOnly(
      const UniqueFD& base_fd, const std::string& sub_path = "");

  static std::unique_ptr<FileMapping> CreateReadExecute(
      const std::string& path);

  static std::unique_ptr<FileMapping> CreateReadExecute(
      const UniqueFD& base_fd, const std::string& sub_path = "");

  // |Mapping|
  size_t GetSize() const override;

  // |Mapping|
  const uint8_t* GetMapping() const override;

  // |Mapping|
  bool IsDontNeedSafe() const override;

  uint8_t* GetMutableMapping();

  bool IsValid() const;

 private:
  bool valid_ = false;
  size_t size_ = 0;
  uint8_t* mapping_ = nullptr;
  uint8_t* mutable_mapping_ = nullptr;

#ifdef SKITY_WIN
  UniqueFD mapping_handle_;
#endif

  SKITY_DISALLOW_COPY_AND_ASSIGN(FileMapping);
};

}  // namespace skity

#endif  // SRC_BASE_MAPPING_HPP
