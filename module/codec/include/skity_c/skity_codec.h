// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CODEC_H
#define MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CODEC_H

// Image-codec entry points (decode / recognize) over the C ABI. Only
// available when skity is built with SKITY_CODEC_MODULE=ON; otherwise
// skity_codec_decode returns NULL and skity_codec_recognize returns
// SKITY_ERROR_NOT_SUPPORTED.
//
// Experiment stage: this mirrors the primary decode path of the C++
// skity::Codec (MakeFromData + Decode); the multi-frame decoder and
// encoding stay C++-only until the terminal C surface is designed.

#include <skity_c/skity_base.h>
#include <skity_c/skity_bitmap.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Recognized image container types. */
typedef enum {
  SKITY_CODEC_TYPE_UNKNOWN = 0,
  SKITY_CODEC_TYPE_PNG,
  SKITY_CODEC_TYPE_JPEG,
  SKITY_CODEC_TYPE_GIF,
  SKITY_CODEC_TYPE_WEBP,
  SKITY_CODEC_TYPE_BMP,
} skity_codec_type;

/**
 * @brief Sniff the image container type from its leading header bytes.
 *
 * @param header  at least @p size bytes of the encoded image
 * @param size    number of bytes available in @p header (>= 12 is enough
 *                for every supported signature)
 * @param out_type  receives the recognized type
 * @return SKITY_SUCCESS, or SKITY_ERROR_INVALID_ARGUMENT
 */
SKITY_C_API skity_result skity_codec_recognize(const uint8_t* header,
                                               size_t size,
                                               skity_codec_type* out_type);

/**
 * @brief Decode an encoded image (PNG / JPEG / GIF first frame / WebP /
 *        BMP) into an unpremul RGBA8888 pixmap.
 *
 * @param data   encoded image bytes
 * @param size   byte length of @p data
 * @param target_width / @param target_height  requested output size;
 *        both 0 decodes at intrinsic size, a zero dimension is derived
 *        from the other preserving aspect ratio, both non-zero form an
 *        aspect-fit box. Never upscales (see the C++ DecodeOptions).
 * @return an owning skity_pixmap handle, or NULL when the container is
 *         unsupported or decoding failed
 */
SKITY_C_API skity_pixmap skity_codec_decode(const void* data, size_t size,
                                            int32_t target_width,
                                            int32_t target_height);

/** @brief Opaque multi-frame image decoder (skity::MultiFrameDecoder). */
SKITY_C_DEFINE_HANDLE(skity_multi_frame_decoder);

/** @brief How a frame's area is treated before the next frame is drawn
 *         (values follow the GIF spec, mirroring skity::CodecDisposalMethod).
 */
typedef enum {
  SKITY_CODEC_DISPOSAL_KEEP = 1,             /**< draw next frame on top */
  SKITY_CODEC_DISPOSAL_RESTORE_BG_COLOR = 2, /**< clear the frame rect to the
                                                  background before the next
                                                  frame */
  SKITY_CODEC_DISPOSAL_RESTORE_PREVIOUS = 3, /**< draw next frame on top of
                                                  the previous frame */
} skity_codec_disposal_method;

/** @brief How a frame blends into the previous one, mirroring
 *         skity::CodecBlendMode. */
typedef enum {
  SKITY_CODEC_BLEND_SRC_OVER = 0, /**< SRC_OVER blend */
  SKITY_CODEC_BLEND_SRC,          /**< SRC (replace) blend */
} skity_codec_blend_mode;

/** @brief Snapshot of a frame's metadata (skity::CodecFrame read access). */
typedef struct skity_codec_frame_info {
  int32_t frame_id;       /**< 0-based index in the image sequence */
  int32_t required_frame; /**< frame this one depends on (-1 = none) */
  int32_t x;              /**< frame rect within the image */
  int32_t y;
  int32_t width;
  int32_t height;
  int32_t duration;        /**< display duration in ms; 0 = as long as
                                possible (single-frame image) */
  uint32_t has_alpha;      /**< non-zero when the frame carries alpha */
  uint32_t fully_received; /**< non-zero when no further data is expected */
  skity_alpha_type alpha_type;
  skity_codec_disposal_method disposal_method;
  skity_codec_blend_mode blend_mode;
} skity_codec_frame_info;

/**
 * @brief Decode a multi-frame image (animated GIF / WebP) into a frame
 *        iterator (skity::Codec::MakeFromData + SetData +
 *        DecodeMultiFrame over the C ABI).
 *
 * @param data  encoded image bytes
 * @param size  byte length of @p data
 * @return new decoder handle, or NULL for single-frame / unsupported input
 */
SKITY_C_API skity_multi_frame_decoder
skity_codec_decode_multi_frame(const void* data, size_t size);

/** @brief Release the decoder handle. Safe on NULL. */
SKITY_C_API void skity_multi_frame_decoder_destroy(
    skity_multi_frame_decoder decoder);

/** @brief Pixel width of the decoded frames. */
SKITY_C_API int32_t
skity_multi_frame_decoder_get_width(skity_multi_frame_decoder decoder);

/** @brief Pixel height of the decoded frames. */
SKITY_C_API int32_t
skity_multi_frame_decoder_get_height(skity_multi_frame_decoder decoder);

/** @brief Number of frames in the image sequence. */
SKITY_C_API int32_t
skity_multi_frame_decoder_get_frame_count(skity_multi_frame_decoder decoder);

/**
 * @brief Fetch a frame's metadata by index.
 *
 * @param decoder  decoder handle
 * @param index    0-based frame index
 * @param out      receives the frame info snapshot
 * @return         1 on success, 0 on an invalid handle / out-of-range index
 */
SKITY_C_API uint32_t skity_multi_frame_decoder_get_frame_info(
    skity_multi_frame_decoder decoder, int32_t index,
    skity_codec_frame_info* out);

/**
 * @brief Decode a single frame to a pixmap.
 *
 * @param decoder  decoder handle
 * @param index    0-based frame index
 * @param prev     pixmap handle holding the previously decoded frame (may
 *                 be NULL for the first frame); when given, the decoder
 *                 reuses its buffer when the format allows
 * @return an owning skity_pixmap handle with the composited frame, or NULL
 *         when decoding failed
 */
SKITY_C_API skity_pixmap skity_multi_frame_decoder_decode_frame(
    skity_multi_frame_decoder decoder, int32_t index, skity_pixmap prev);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MODULE_CAPI_INCLUDE_SKITY_C_SKITY_CODEC_H
