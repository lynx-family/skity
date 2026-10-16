// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// C ABI for the image codec module. Compiled only when SKITY_CODEC_MODULE
// is on: in the normal build it is part of skity-capi (linking
// skity-codec); in the unified-lib experiment the codec is folded into
// libskity, so the entry points resolve inside the single .so.

#include <skity_c/skity_codec.h>

#include <cstring>
#include <skity/codec/codec.hpp>
#include <skity/io/data.hpp>

#include "handle.hpp"

namespace {

bool starts_with(const uint8_t* h, size_t size, const uint8_t* sig,
                 size_t sig_len) {
  return size >= sig_len && memcmp(h, sig, sig_len) == 0;
}

skity::MultiFrameDecoder* decoder_of(skity_multi_frame_decoder handle) {
  auto* w = skity::capi::resolve<skity_multi_frame_decoder_s>(
      handle, SKITY_OBJECT_TYPE_MULTI_FRAME_DECODER);
  return w ? static_cast<skity::MultiFrameDecoder*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_result skity_codec_recognize(const uint8_t* header, size_t size,
                                   skity_codec_type* out_type) {
  if (header == nullptr || out_type == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  *out_type = SKITY_CODEC_TYPE_UNKNOWN;
  if (starts_with(header, size, reinterpret_cast<const uint8_t*>("\x89PNG"),
                  4)) {
    *out_type = SKITY_CODEC_TYPE_PNG;
  } else if (starts_with(header, size,
                         reinterpret_cast<const uint8_t*>("\xFF\xD8\xFF"), 3)) {
    *out_type = SKITY_CODEC_TYPE_JPEG;
  } else if (starts_with(header, size, reinterpret_cast<const uint8_t*>("GIF8"),
                         4)) {
    *out_type = SKITY_CODEC_TYPE_GIF;
  } else if (size >= 12 &&
             starts_with(header, 4, reinterpret_cast<const uint8_t*>("RIFF"),
                         4) &&
             memcmp(header + 8, "WEBP", 4) == 0) {
    *out_type = SKITY_CODEC_TYPE_WEBP;
  } else if (starts_with(header, size, reinterpret_cast<const uint8_t*>("BM"),
                         2)) {
    *out_type = SKITY_CODEC_TYPE_BMP;
  }
  return SKITY_SUCCESS;
}

skity_pixmap skity_codec_decode(const void* data, size_t size,
                                int32_t target_width, int32_t target_height) {
  if (data == nullptr || size == 0) {
    return nullptr;
  }
  auto bytes = skity::Data::MakeWithCopy(data, size);
  auto codec = skity::Codec::MakeFromData(bytes);
  if (codec == nullptr) {
    return nullptr;
  }
  // MakeFromData() returns codec->Fork() without attaching the data, so
  // Decode() would always see an empty data_ (pre-existing codec-module
  // bug); re-attach it here.
  codec->SetData(bytes);
  skity::DecodeOptions options{};
  options.target_width = target_width;
  options.target_height = target_height;
  auto pixmap = codec->Decode(options);
  if (pixmap == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_pixmap_s>(
      SKITY_OBJECT_TYPE_PIXMAP, SKITY_HANDLE_OWNING, std::move(pixmap));
}

skity_multi_frame_decoder skity_codec_decode_multi_frame(const void* data,
                                                         size_t size) {
  if (data == nullptr || size == 0) {
    return nullptr;
  }
  auto bytes = skity::Data::MakeWithCopy(data, size);
  auto codec = skity::Codec::MakeFromData(bytes);
  if (codec == nullptr) {
    return nullptr;
  }
  // MakeFromData() returns codec->Fork() without attaching the data (see
  // skity_codec_decode); re-attach it before decoding.
  codec->SetData(bytes);
  auto decoder = codec->DecodeMultiFrame();
  if (decoder == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_multi_frame_decoder_s>(
      SKITY_OBJECT_TYPE_MULTI_FRAME_DECODER, SKITY_HANDLE_OWNING,
      std::move(decoder));
}

void skity_multi_frame_decoder_destroy(skity_multi_frame_decoder decoder) {
  skity::capi::destroy_handle<skity_multi_frame_decoder_s>(
      decoder, SKITY_OBJECT_TYPE_MULTI_FRAME_DECODER);
}

int32_t skity_multi_frame_decoder_get_width(skity_multi_frame_decoder decoder) {
  auto* d = decoder_of(decoder);
  return d ? d->GetWidth() : 0;
}

int32_t skity_multi_frame_decoder_get_height(
    skity_multi_frame_decoder decoder) {
  auto* d = decoder_of(decoder);
  return d ? d->GetHeight() : 0;
}

int32_t skity_multi_frame_decoder_get_frame_count(
    skity_multi_frame_decoder decoder) {
  auto* d = decoder_of(decoder);
  return d ? d->GetFrameCount() : 0;
}

uint32_t skity_multi_frame_decoder_get_frame_info(
    skity_multi_frame_decoder decoder, int32_t index,
    skity_codec_frame_info* out) {
  auto* d = decoder_of(decoder);
  if (d == nullptr || out == nullptr) {
    return 0u;
  }
  const skity::CodecFrame* frame = d->GetFrameInfo(index);
  if (frame == nullptr) {
    return 0u;
  }
  out->frame_id = frame->GetFrameID();
  out->required_frame = frame->GetRequiredFrame();
  out->x = frame->GetX();
  out->y = frame->GetY();
  out->width = frame->GetWidth();
  out->height = frame->GetHeight();
  out->duration = frame->GetDuration();
  out->has_alpha = frame->HasAlpha() ? 1u : 0u;
  out->fully_received = frame->IsFullyReceived() ? 1u : 0u;
  out->alpha_type = static_cast<skity_alpha_type>(frame->GetAlphaType());
  out->disposal_method =
      static_cast<skity_codec_disposal_method>(frame->GetDisposalMethod());
  out->blend_mode = static_cast<skity_codec_blend_mode>(frame->GetBlendMode());
  return 1u;
}

skity_pixmap skity_multi_frame_decoder_decode_frame(
    skity_multi_frame_decoder decoder, int32_t index, skity_pixmap prev) {
  auto* d = decoder_of(decoder);
  if (d == nullptr) {
    return nullptr;
  }
  const skity::CodecFrame* frame = d->GetFrameInfo(index);
  if (frame == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::Pixmap> prev_impl;
  if (prev != nullptr) {
    prev_impl = skity::capi::get_impl<skity_pixmap_s, skity::Pixmap>(
        prev, SKITY_OBJECT_TYPE_PIXMAP);
    if (prev_impl == nullptr) {
      return nullptr;
    }
  }
  auto pixmap = d->DecodeFrame(frame, std::move(prev_impl));
  if (pixmap == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_pixmap_s>(
      SKITY_OBJECT_TYPE_PIXMAP, SKITY_HANDLE_OWNING, std::move(pixmap));
}

}  // extern "C"
