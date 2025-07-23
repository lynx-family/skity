// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/token.h"

namespace wgx {

double Token::ToF64() const {
  auto p = std::get_if<double>(&value);
  if (p) {
    return *p;
  }

  return 0.0;
}

int64_t Token::ToI64() const {
  auto p = std::get_if<int64_t>(&value);
  if (p) {
    return *p;
  }

  return 0;
}

std::string_view Token::ToString() const {
  if (type != TokenType::kIdentifier) {
    return {};
  }

  auto view = std::get_if<std::string_view>(&value);

  if (view) {
    return *view;
  }

  return {};
}

}  // namespace wgx