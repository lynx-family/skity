// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "common/golden_texture.hpp"

#include "common/golden_test_env.hpp"

namespace skity {
namespace testing {

std::shared_ptr<Pixmap> GoldenTexture::ReadPixels() {
  auto ctx = GoldenTestEnv::GetInstance()->GetGPUContext();

  if (ctx == nullptr) {
    return {};
  }

  return image_->ReadPixels(ctx);
}

}  // namespace testing
}  // namespace skity
