// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_FILE_HPP
#define SRC_BASE_FILE_HPP

#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "src/base/base_macros.hpp"
#include "src/base/unique_fd.hpp"

#ifdef ERROR
#undef ERROR
#endif

namespace skity {

class Mapping;

enum class FilePermission {
  kRead,
  kWrite,
  kReadWrite,
};

/// This can open a directory on POSIX, but not on Windows.
UniqueFD OpenFile(const char* path, bool create_if_necessary,
                  FilePermission permission);

/// This can open a directory on POSIX, but not on Windows.
UniqueFD OpenFile(const UniqueFD& base_directory, const char* path,
                  bool create_if_necessary, FilePermission permission);

bool FileExists(const UniqueFD& base_directory, const char* path);

}  // namespace skity

#endif  // SRC_BASE_FILE_HPP
