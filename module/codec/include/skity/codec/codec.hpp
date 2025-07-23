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
 * Codec interface
 */
class SKITY_EXPERIMENTAL_API Codec {
 public:
  Codec() = default;
  virtual ~Codec() = default;
  Codec(const Codec&) = delete;
  Codec& operator=(const Codec&) = delete;

  virtual std::shared_ptr<Pixmap> Decode() = 0;

  virtual std::shared_ptr<Data> Encode(const Pixmap* pixmap) = 0;

  virtual bool RecognizeFileType(const char* header, size_t size) = 0;

  void SetData(std::shared_ptr<Data> data) { data_ = std::move(data); }

  static std::shared_ptr<Codec> MakeFromData(std::shared_ptr<Data> const& data);

#ifdef SKITY_HAS_PNG
  static std::shared_ptr<Codec> MakePngCodec();
#endif

#ifdef SKITY_HAS_JPEG
  static std::shared_ptr<Codec> MakeJPEGCodec();
#endif

 protected:
  std::shared_ptr<Data> data_;

 private:
  static void SetupCodecs();
};

}  // namespace skity

#endif  // MODULE_CODEC_INCLUDE_SKITY_CODEC_CODEC_HPP
