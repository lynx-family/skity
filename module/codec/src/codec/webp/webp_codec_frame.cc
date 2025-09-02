// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/webp/webp_codec_frame.hpp"

namespace skity {

WebpFrame::WebpFrame(int32_t i, const WebPIterator& iter) : CodecFrame(i, {}) {
  SetXYWH(iter.x_offset, iter.y_offset, iter.width, iter.height);

  SetDisposalMethod(iter.dispose_method == WEBP_MUX_DISPOSE_BACKGROUND
                        ? CodecDisposalMethod::RestoreBGColor
                        : CodecDisposalMethod::Keep);

  SetDuration(iter.duration);

  if (iter.blend_method != WEBP_MUX_BLEND) {
    SetBlendMode(CodecBlendMode::Src);
  }

  SetAlphaType(iter.has_alpha ? AlphaType::kPremul_AlphaType
                              : AlphaType::kOpaque_AlphaType);
}

}  // namespace skity
