// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <optional>
#include <sstream>
#include <string_view>

namespace wgx {
namespace msl {

struct Attribute {
  std::string_view name;
  std::optional<std::string_view> prefix;
  std::optional<uint32_t> location;

  Attribute(std::string_view name) : name(name), prefix(), location() {}

  Attribute(std::string_view name, uint32_t location)
      : name(name), prefix(), location(location) {}

  Attribute(std::string_view name, std::string_view prefix, uint32_t location)
      : name(name), prefix(prefix), location(location) {}
};

std::ostream& operator<<(std::ostream& os, const Attribute& attribute);

}  // namespace msl
}  // namespace wgx
