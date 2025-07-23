/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_FIXED_TYPES_HPP
#define SRC_BASE_FIXED_TYPES_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <skity/render/canvas.hpp>
#include <utility>

namespace skity {

typedef int32_t FixedDot16;
typedef int32_t FixedDot6;
typedef int32_t FixedDot14;

#define FixedDot16_1 (1 << 16)

#define MaxS32FitsInFloat 2147483520
#define MinS32FitsInFloat -MaxS32FitsInFloat

static inline int float_saturate2int(float x) {
  x = x < MaxS32FitsInFloat ? x : MaxS32FitsInFloat;
  x = x > MinS32FitsInFloat ? x : MinS32FitsInFloat;
  return static_cast<int>(x);
}

#define FixedDot16ToFloat(x) ((x) * 1.52587890625e-5f)
#define FloatToFixedDot16(x) float_saturate2int((x) * FixedDot16_1)

#define IntToFixedDot6(x) ((x) << 6)
#define FloatToFixedDot6(x) (FixedDot6)((x) * 64)
#define FixedDot6ToFloat(x) ((float)(x) * 0.015625f)  // NOLINT

#define FixedDot14ToFloat(x) ((x) / (float)(1 << 14))  // NOLINT

/** SkScopeExit calls a std:::function<void()> in its destructor. */
class SkScopeExit {
 public:
  SkScopeExit() = default;
  explicit SkScopeExit(std::function<void()> f) : fFn(std::move(f)) {}
  SkScopeExit(SkScopeExit&& that) : fFn(std::move(that.fFn)) {}

  ~SkScopeExit() {
    if (fFn) {
      fFn();
    }
  }

  void clear() { fFn = {}; }

  SkScopeExit& operator=(SkScopeExit&& that) {
    fFn = std::move(that.fFn);
    return *this;
  }

 private:
  std::function<void()> fFn;

  SkScopeExit(const SkScopeExit&) = delete;
  SkScopeExit& operator=(const SkScopeExit&) = delete;
};

/**
 * SK_AT_SCOPE_EXIT(stmt) evaluates stmt when the current scope ends.
 *
 * E.g.
 *    {
 *        int x = 5;
 *        {
 *           SK_AT_SCOPE_EXIT(x--);
 *           SkASSERT(x == 5);
 *        }
 *        SkASSERT(x == 4);
 *    }
 */
#define SK_MACRO_CONCAT(X, Y) SK_MACRO_CONCAT_IMPL_PRIV(X, Y)
#define SK_MACRO_CONCAT_IMPL_PRIV(X, Y) X##Y
#define SK_MACRO_APPEND_LINE(name) SK_MACRO_CONCAT(name, __LINE__)
#define SK_AT_SCOPE_EXIT(stmt) \
  SkScopeExit SK_MACRO_APPEND_LINE(at_scope_exit_)([&]() { stmt; })

}  // namespace skity

#endif  // SRC_BASE_FIXED_TYPES_HPP
