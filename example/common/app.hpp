// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SKITY_EXAMPLE_COMMON_APP_HPP
#define SKITY_EXAMPLE_COMMON_APP_HPP

#include "common/window.hpp"

namespace skity {
namespace example {

int StartExampleApp(int argc, const char** argv, WindowClient& client,
                    int width, int height, std::string title);

}
}  // namespace skity

#endif  // SKITY_EXAMPLE_COMMON_APP_HPP
