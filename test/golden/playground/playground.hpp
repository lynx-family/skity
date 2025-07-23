// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <skity/skity.hpp>

namespace skity {
namespace testing {

class GoldenTexture;

bool OpenPlayground(bool passed, std::shared_ptr<GoldenTexture> source,
                    std::shared_ptr<Pixmap> target, const char* golden_path);

}  // namespace testing
}  // namespace skity
