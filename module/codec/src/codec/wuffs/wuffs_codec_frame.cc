// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/wuffs/wuffs_codec_frame.hpp"

namespace skity {

WuffsCodecFrame::WuffsCodecFrame(const wuffs_base__frame_config& config)
    : CodecFrame(config.index(), {}), io_pos_(config.io_position()) {
  auto rect = config.bounds();

  SetXYWH(rect.min_incl_x, rect.min_incl_y, rect.width(), rect.height());
  SetDisposalMethod(WuffsToCodecDisposalMethod(config.disposal()));
  SetDuration(config.duration() / WUFFS_BASE__FLICKS_PER_MILLISECOND);
  SetBlendMode(config.overwrite_instead_of_blend() ? CodecBlendMode::Src
                                                   : CodecBlendMode::SrcOver);
  SetAlphaType(config.opaque_within_bounds() ? AlphaType::kOpaque_AlphaType
                                             : AlphaType::kUnpremul_AlphaType);
}

}  // namespace skity
