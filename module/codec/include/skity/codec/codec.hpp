// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP
#define MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP

#include <memory>
#include <skity/macros.hpp>
#include <utility>

namespace skity {

class Data;
class Pixmap;

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
   *
   * @return The decoded pixmap. nullptr if decode failed.
   */
  virtual std::shared_ptr<Pixmap> Decode() = 0;

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

 protected:
  std::shared_ptr<Data> data_;

 private:
  static void SetupCodecs();
};

}  // namespace skity

#endif  // MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP
