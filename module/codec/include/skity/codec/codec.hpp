// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP
#define MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP

#include <memory>
#include <optional>
#include <skity/graphic/alpha_type.hpp>
#include <skity/macros.hpp>
#include <utility>
#include <vector>

namespace skity {

class Data;
class Pixmap;

/**
 * The rectangle of a frame in the image.
 *
 * The rectangle is defined as {left, top, right, bottom}.
 */
struct SKITY_API CodecRect {
  int32_t left = 0;
  int32_t top = 0;
  int32_t right = 0;
  int32_t bottom = 0;

  CodecRect() = default;

  CodecRect(int32_t left, int32_t top, int32_t right, int32_t bottom)
      : left(left), top(top), right(right), bottom(bottom) {}

  bool operator==(const CodecRect& other) const {
    return left == other.left && top == other.top && right == other.right &&
           bottom == other.bottom;
  }

  bool operator!=(const CodecRect& other) const { return !(*this == other); }

  void SetXYWH(int32_t x, int32_t y, int32_t width, int32_t height) {
    left = x;
    top = y;
    right = x + width;
    bottom = y + height;
  }

  int32_t Width() const { return right - left; }
  int32_t Height() const { return bottom - top; }

  int32_t X() const { return left; }
  int32_t Y() const { return top; }

  int32_t Right() const { return right; }
  int32_t Bottom() const { return bottom; }

  bool IsEmpty() const { return left >= right || top >= bottom; }

  bool Intersect(const CodecRect& other) { return Intersect(*this, other); }

  bool Intersect(const CodecRect& a, const CodecRect& b) {
    CodecRect temp{
        std::max(a.left, b.left),
        std::max(a.top, b.top),
        std::min(a.right, b.right),
        std::min(a.bottom, b.bottom),
    };

    if (temp.IsEmpty()) {
      return false;
    }

    *this = temp;

    return true;
  }

  bool Contains(const CodecRect& other) const {
    return !other.IsEmpty() && !this->IsEmpty() && left <= other.left &&
           top <= other.top && right >= other.right && bottom >= other.bottom;
  }
};

/**
 * This specifies how the next frame is based on this frame.
 *
 * Names are based on the GIF spec.
 *
 * The numbers coorespond to values in GIF file.
 */
enum class CodecDisposalMethod {
  /**
   * The next frame should be drawn on top of this one.
   *
   * In a GIF, a value of 0 is also thread as Keep
   */
  Keep = 1,

  /**
   * Keep all other pixels except the area inside this frame's rectangle.
   * The rectangle area of this frame should be cleared to the background color
   * (transparent). Before drawing the next frame.
   */
  RestoreBGColor = 2,

  /**
   * The next frame should be drawn on top of the previous frame.
   *
   * In a GIF, a value of 4 is also thread as RestorePrevious.
   */
  RestorePrevious = 3,
};

/**
 * The blend mode to use when drawing this frame.
 */
enum class CodecBlendMode {
  /**
   * Blend this frame with the previous frame using the SRC_OVER blend mode.
   */
  SrcOver,

  /**
   * Blend this frame with the previous frame using the SRC blend mode.
   */
  Src,
};

/**
 * Information about a frame in a multi-frame image.
 */
struct CodecFrameInfo {
  enum {
    kNoFrameRequired = -1,
  };

  /**
   * The frame that this frame needs to be blended with.
   * If kNoFrameRequired, this frame is independent means can be draw to an
   * uninitialized buffer.
   *
   * Note: This is the *earlier* frame that this frame depends on. Any frame
   * from [required_frame, this_frame) can be used, unless its disposal method
   * is kRestorePrevious.
   */
  int32_t required_frame = kNoFrameRequired;

  /**
   * The duration of this frame to show in milliseconds.
   */
  int32_t duration = 0;

  bool fully_received = false;

  /**
   * The alpha type of the frame.
   */
  AlphaType alpha_type = AlphaType::kUnknown_AlphaType;

  /**
   * Whether this frame has alpha channel.
   */
  bool has_alpha = false;

  /**
   * The disposal method of this frame.
   */
  CodecDisposalMethod disposal_method = CodecDisposalMethod::Keep;

  /**
   * The blend mode of this frame.
   */
  CodecBlendMode blend_mode = CodecBlendMode::SrcOver;

  /**
   * The rectangle of this frame in the image.
   */
  CodecRect rect = {0, 0, 0, 0};
};

/**
 * A frame in a multi-frame image.
 */
class SKITY_API CodecFrame {
 public:
  CodecFrame(int32_t id, CodecFrameInfo info)
      : id_(id), info_(std::move(info)) {}

  virtual ~CodecFrame() = default;

  /**
   * 0 based index of the frame in the image sequence.
   */
  int32_t GetFrameID() const { return id_; }

  const CodecFrameInfo& GetInfo() const { return info_; }

  /**
   * Whether this frame has alpha channel.
   */
  bool HasAlpha() const { return info_.has_alpha; }
  void SetHasAlpha(bool has_alpha) { info_.has_alpha = has_alpha; }

  bool ReachedStart() const {
    return info_.required_frame == CodecFrameInfo::kNoFrameRequired;
  }

  /**
   * Get the frame ID that this frame depends on.
   */
  int32_t GetRequiredFrame() const { return info_.required_frame; }
  void SetRequiredFrame(int32_t required_frame) {
    info_.required_frame = required_frame;
  }

  /**
   * Set the rectangle of this frame in the image.
   */
  void SetXYWH(int32_t x, int32_t y, int32_t width, int32_t height) {
    info_.rect.SetXYWH(x, y, width, height);
  }
  const CodecRect& GetRect() const { return info_.rect; }
  int32_t GetX() const { return info_.rect.X(); }
  int32_t GetY() const { return info_.rect.Y(); }
  int32_t GetWidth() const { return info_.rect.Width(); }
  int32_t GetHeight() const { return info_.rect.Height(); }

  CodecDisposalMethod GetDisposalMethod() const {
    return info_.disposal_method;
  }
  void SetDisposalMethod(CodecDisposalMethod disposal_method) {
    info_.disposal_method = disposal_method;
  }

  /**
   * Get the duration of this frame to show in milliseconds.
   *
   * @return The duration of this frame to show in milliseconds. 0 means this
   *         frame should be shown as long as possible. And the image should
   *         only contain only one frame.
   */
  int32_t GetDuration() const { return info_.duration; }
  void SetDuration(int32_t duration) { info_.duration = duration; }

  CodecBlendMode GetBlendMode() const { return info_.blend_mode; }
  void SetBlendMode(CodecBlendMode blend_mode) {
    info_.blend_mode = blend_mode;
  }

  bool IsFullyReceived() const { return info_.fully_received; }
  void SetFullyReceived(bool fully_received) {
    info_.fully_received = fully_received;
  }

  AlphaType GetAlphaType() const { return info_.alpha_type; }
  void SetAlphaType(AlphaType alpha_type) { info_.alpha_type = alpha_type; }

 private:
  int32_t id_;
  CodecFrameInfo info_;
};

/**
 * A decoder for multi-frame image. If the image is a multi-frame image format
 * and the codec support decode multi-frame, this class will be returned from
 * Codec::DecodeMultiFrame().
 *
 * @note The is a experimental API. The API is unstable and may change in the
 * future.
 */
class SKITY_EXPERIMENTAL_API MultiFrameDecoder {
 public:
  virtual ~MultiFrameDecoder() = default;

  /**
   * Get the image width of the multi-frame image.
   *
   * @note This size is the image size, not the frame size. In a multi-frame
   * image, the frame may have different size.
   */
  virtual int32_t GetWidth() const = 0;

  /**
   * Get the image height of the multi-frame image.
   *
   * @note This size is the image size, not the frame size. In a multi-frame
   * image, the frame may have different size.
   */
  virtual int32_t GetHeight() const = 0;

  /**
   * Get the frame count of the multi-frame image.
   *
   * @return The frame count of the multi-frame image. This value is always
   * greater than 0.
   */
  virtual int32_t GetFrameCount() const = 0;

  /**
   * Get the frame info of the specified frame.
   *
   * @param frame_id The frame ID to get. The frame_id is 0 based index.
   *
   * @return The frame info of the specified frame. nullptr if frame_id is
   *         invalid.
   */
  virtual const CodecFrame* GetFrameInfo(int32_t frame_id) const = 0;

  /**
   * Decode the specified frame to pixmap.
   *
   * @param frame The frame to decode.
   * @param prev_pixmap The previous pixmap. If this is the first frame, this
   *                    parameter can be null.
   *
   * @return The decoded pixmap. nullptr if decode failed.
   */
  virtual std::shared_ptr<Pixmap> DecodeFrame(
      const CodecFrame* frame, std::shared_ptr<Pixmap> prev_pixmap) = 0;

 protected:
  void SetAlphaAndRequiredFrame(CodecFrame* frame);
};

/**
 * Codec interface for encoding and decoding image data.
 *
 * @note This is experimental the API is unstable.
 */
class SKITY_EXPERIMENTAL_API Codec {
 public:
  Codec() = default;
  virtual ~Codec() = default;
  Codec(const Codec&) = delete;
  Codec& operator=(const Codec&) = delete;

  /**
   * Encode the raw pixmap to codec specific data.
   *
   * @param pixmap The raw pixmap to encode. Must not be null.
   *
   * @return The encoded data. nullptr if encode failed.
   */
  virtual std::shared_ptr<Data> Encode(const Pixmap* pixmap) = 0;

  /**
   * Recognize the file type from header.
   *
   * @param header The header of the file.
   * @param size   The size of the header data.
   * @return true if the file type is supported by this codec.
   */
  virtual bool RecognizeFileType(const char* header, size_t size) = 0;

  /**
   * Set the data to be decoded or encoded.
   *
   * @param data The data to be decoded. Must not be null.
   */
  void SetData(std::shared_ptr<Data> data) { data_ = std::move(data); }

  /**
   * Decode the data to pixmap.
   * If decode a multi-frame image, this method will return the first frame.
   * To get other frames, use DecodeMultiFrame().
   *
   * @note Currently the codec only output pixmap with RGBA color type and
   *       unpremul alpha type.
   *
   * @return The decoded pixmap. nullptr if decode failed.
   */
  virtual std::shared_ptr<Pixmap> Decode() = 0;

  /**
   * Decode the data to pixmap. If decode a multi-frame image, this method will
   * return the pixmap of the specified frame.
   *
   * @param frame The frame to decode.
   * @return The decoded pixmap. nullptr if decode failed or this image is not
   *         a multi-frame image.
   */
  virtual std::shared_ptr<MultiFrameDecoder> DecodeMultiFrame() = 0;

  /**
   * Create a codec from data. Will try to recognize the file type and create
   * the corresponding codec.
   *
   * @param data The data to be decoded.
   * @return The codec. nullptr if create failed or file type is not supported.
   */
  static std::shared_ptr<Codec> MakeFromData(std::shared_ptr<Data> const& data);

  static std::shared_ptr<Codec> MakePngCodec();

  static std::shared_ptr<Codec> MakeJPEGCodec();

  static std::shared_ptr<Codec> MakeGIFCodec();

  static std::shared_ptr<Codec> MakeWebpCodec();

 protected:
  std::shared_ptr<Data> data_;

 private:
  static void SetupCodecs();
};

}  // namespace skity

#endif  // MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP
