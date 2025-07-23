// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/base/file.hpp"

namespace skity {

UniqueFD OpenFile(const UniqueFD& base_directory, const char* path,
                  bool create_if_necessary, FilePermission permission) {
  return {};
}

UniqueFD OpenFile(const char* path, bool create_if_necessary,
                  FilePermission permission) {
  return {};
}

}  // namespace skity
