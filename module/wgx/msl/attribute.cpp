// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "msl/attribute.h"

namespace wgx {
namespace msl {

std::ostream& operator<<(std::ostream& os, const Attribute& attribute) {
  os << attribute.name;

  if (attribute.location) {
    os << "(";
    if (attribute.prefix) {
      os << *attribute.prefix;
    }
    os << *attribute.location << ")";
  }

  return os;
}

}  // namespace msl
}  // namespace wgx
