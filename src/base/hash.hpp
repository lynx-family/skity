/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_HASH_HPP
#define SRC_BASE_HASH_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

namespace skity {

/**
 * This is a fast, high-quality 32-bit hash. We make no guarantees about this
 * remaining stable over time, or being consistent across devices.
 *
 * For now, this is a 64-bit wyhash, truncated to 32-bits.
 * See: https://github.com/wangyi-fudan/wyhash
 */
uint32_t Hash32(const void* data, size_t bytes, uint32_t seed = 0);

}  // namespace skity

#endif  // SRC_BASE_HASH_HPP
