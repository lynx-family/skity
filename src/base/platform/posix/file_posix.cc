// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dirent.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <sstream>

#include "src/base/eintr_wrapper.hpp"
#include "src/base/file.hpp"
#include "src/base/mapping.hpp"
#include "src/base/unique_fd.hpp"

namespace skity {

static int ToPosixAccessFlags(FilePermission permission) {
  int flags = 0;
  switch (permission) {
    case FilePermission::kRead:
      flags |= O_RDONLY;  // read only
      break;
    case FilePermission::kWrite:
      flags |= O_WRONLY;  // write only
      break;
    case FilePermission::kReadWrite:
      flags |= O_RDWR;  // read-write
      break;
  }
  return flags;
}

static int ToPosixCreateModeFlags(FilePermission permission) {
  int mode = 0;
  switch (permission) {
    case FilePermission::kRead:
      mode |= S_IRUSR;
      break;
    case FilePermission::kWrite:
      mode |= S_IWUSR;
      break;
    case FilePermission::kReadWrite:
      mode |= S_IRUSR | S_IWUSR;
      break;
  }
  return mode;
}

UniqueFD OpenFile(const char* path, bool create_if_necessary,
                  FilePermission permission) {
  return OpenFile(UniqueFD{AT_FDCWD}, path, create_if_necessary, permission);
}

UniqueFD OpenFile(const UniqueFD& base_directory, const char* path,
                  bool create_if_necessary, FilePermission permission) {
  if (path == nullptr) {
    return {};
  }

  int flags = 0;
  int mode = 0;

  if (create_if_necessary && !FileExists(base_directory, path)) {
    flags = ToPosixAccessFlags(permission) | O_CREAT | O_TRUNC;
    mode = ToPosixCreateModeFlags(permission);
  } else {
    flags = ToPosixAccessFlags(permission);
    mode = 0;  // Not creating since it already exists.
  }

  return UniqueFD{
      SKITY_HANDLE_EINTR(::openat(base_directory.get(), path, flags, mode))};
}

bool FileExists(const UniqueFD& base_directory, const char* path) {
  if (!base_directory.is_valid()) {
    return false;
  }

  return ::faccessat(base_directory.get(), path, F_OK, 0) == 0;
}

}  // namespace skity
