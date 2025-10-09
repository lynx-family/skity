// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// clang-format off
#include "src/base/platform/win/lean_windows.hpp"
#include "src/base/platform/win/str_conversion.hpp"
#include <Fileapi.h>
#include <Shlwapi.h>
// clang-format on
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>

#include <algorithm>
#include <codecvt>
#include <locale>
#include <sstream>
#include <string>

#include "src/base/file.hpp"
#include "src/base/unique_fd.hpp"

namespace skity {

static std::string GetFullHandlePath(const UniqueFD& handle) {
  wchar_t buffer[MAX_PATH] = {0};
  const DWORD buffer_size = ::GetFinalPathNameByHandleW(
      handle.get(), buffer, MAX_PATH, FILE_NAME_NORMALIZED);
  if (buffer_size == 0) {
    return {};
  }
  std::wstring w_path{buffer, buffer_size};
  std::string path;
  if (FAILED(StrConversion::WideStringToString(w_path, &path))) {
    return {};
  }
  return path;
}

static bool IsAbsolutePath(const char* path) {
  if (path == nullptr || strlen(path) == 0) {
    return false;
  }

  std::string utf8_path{path, strlen(path)};
  std::wstring w_path;
  HRB(StrConversion::StringToWideString(utf8_path, &w_path));
  if (w_path.empty()) {
    return false;
  }

  return ::PathIsRelativeW(w_path.c_str()) == FALSE;
}

static std::string GetAbsolutePath(const UniqueFD& base_directory,
                                   const char* subpath) {
  std::string path;
  if (IsAbsolutePath(subpath)) {
    path = subpath;
  } else {
    std::stringstream stream;
    stream << GetFullHandlePath(base_directory) << "\\" << subpath;
    path = stream.str();
  }
  std::replace(path.begin(), path.end(), '/', '\\');
  return path;
}

static DWORD GetDesiredAccessFlags(FilePermission permission) {
  switch (permission) {
    case FilePermission::kRead:
      return GENERIC_READ;
    case FilePermission::kWrite:
      return GENERIC_WRITE;
    case FilePermission::kReadWrite:
      return GENERIC_READ | GENERIC_WRITE;
  }
  return GENERIC_READ;
}

static DWORD GetShareFlags(FilePermission permission) {
  switch (permission) {
    case FilePermission::kRead:
      return FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    case FilePermission::kWrite:
      return 0;
    case FilePermission::kReadWrite:
      return 0;
  }
  return FILE_SHARE_READ;
}

UniqueFD OpenFile(const UniqueFD& base_directory, const char* path,
                  bool create_if_necessary, FilePermission permission) {
  return OpenFile(GetAbsolutePath(base_directory, path).c_str(),
                  create_if_necessary, permission);
}

UniqueFD OpenFile(const char* path, bool create_if_necessary,
                  FilePermission permission) {
  if (path == nullptr || strlen(path) == 0) {
    return {};
  }

  std::string utf8_path{path, strlen(path)};
  std::wstring w_path;
  if (FAILED(StrConversion::StringToWideString(utf8_path, &w_path))) {
    return {};
  }

  if (w_path.empty()) {
    return {};
  }

  const DWORD creation_disposition =
      create_if_necessary ? OPEN_ALWAYS : OPEN_EXISTING;

  const DWORD flags = FILE_ATTRIBUTE_NORMAL;

  auto handle = CreateFileW(w_path.c_str(), GetDesiredAccessFlags(permission),
                            GetShareFlags(permission), nullptr,
                            creation_disposition, flags, nullptr);

  if (handle == INVALID_HANDLE_VALUE) {
    return {};
  }

  return UniqueFD{handle};
}

}  // namespace skity
