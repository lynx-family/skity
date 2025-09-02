// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/codec/codec.hpp>

namespace skity {

namespace {

CodecRect frame_rect_on_screen(CodecRect frame_rect,
                               const CodecRect& screen_rect) {
  if (!frame_rect.Intersect(screen_rect)) {
    return {};
  }

  return frame_rect;
}

}  // namespace

void MultiFrameDecoder::SetAlphaAndRequiredFrame(CodecFrame* frame) {
  bool reports_alpha = frame->GetAlphaType() != AlphaType::kOpaque_AlphaType;
  CodecRect screen_rect{};
  screen_rect.SetXYWH(0, 0, GetWidth(), GetHeight());

  auto frame_rect = frame_rect_on_screen(frame->GetRect(), screen_rect);

  auto i = frame->GetFrameID();

  if (i == 0) {
    frame->SetHasAlpha(reports_alpha || frame_rect != screen_rect);
    frame->SetRequiredFrame(CodecFrameInfo::kNoFrameRequired);  // IND1

    return;
  }

  auto blend_with_prev = frame->GetBlendMode() == CodecBlendMode::SrcOver;
  if ((!reports_alpha || !blend_with_prev) && frame_rect == screen_rect) {
    frame->SetHasAlpha(reports_alpha);
    frame->SetRequiredFrame(CodecFrameInfo::kNoFrameRequired);  // IND2

    return;
  }

  auto prev_frame = GetFrameInfo(i - 1);

  while (prev_frame->GetDisposalMethod() ==
         CodecDisposalMethod::RestorePrevious) {
    auto prev_id = prev_frame->GetFrameID();

    if (prev_id == 0) {
      frame->SetHasAlpha(true);
      frame->SetRequiredFrame(CodecFrameInfo::kNoFrameRequired);  // IND3
      return;
    }

    prev_frame = GetFrameInfo(prev_id - 1);
  }

  auto clear_prev_frame =
      frame->GetDisposalMethod() == CodecDisposalMethod::RestoreBGColor;
  auto prev_frame_rect =
      frame_rect_on_screen(prev_frame->GetRect(), screen_rect);

  if (clear_prev_frame) {
    if (prev_frame_rect == screen_rect ||
        frame->GetRequiredFrame() == CodecFrameInfo::kNoFrameRequired) {
      frame->SetHasAlpha(true);
      frame->SetRequiredFrame(CodecFrameInfo::kNoFrameRequired);  // IND4

      return;
    }
  }

  if (reports_alpha && blend_with_prev) {
    frame->SetRequiredFrame(prev_frame->GetFrameID());  // DEPS
    frame->SetHasAlpha(prev_frame->HasAlpha() || clear_prev_frame);

    return;
  }

  while (frame_rect.Contains(prev_frame_rect)) {
    auto prev_required_frame = prev_frame->GetRequiredFrame();

    if (prev_required_frame == CodecFrameInfo::kNoFrameRequired) {
      frame->SetRequiredFrame(CodecFrameInfo::kNoFrameRequired);  // IND6
      frame->SetHasAlpha(true);

      return;
    }

    prev_frame = GetFrameInfo(prev_required_frame);

    prev_frame_rect = frame_rect_on_screen(prev_frame->GetRect(), screen_rect);
  }

  frame->SetRequiredFrame(prev_frame->GetFrameID());  // IND7

  if (prev_frame->GetDisposalMethod() == CodecDisposalMethod::RestoreBGColor) {
    frame->SetHasAlpha(true);

    return;
  }

  frame->SetHasAlpha(prev_frame->HasAlpha() ||
                     (reports_alpha && !blend_with_prev));
}

}  // namespace skity
