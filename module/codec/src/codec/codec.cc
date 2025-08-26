// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/codec/codec.hpp>
#include <skity/io/data.hpp>
#include <vector>

#include "src/codec/gif_codec.hpp"
#include "src/codec/jpeg_codec.hpp"
#include "src/codec/png_codec.hpp"

namespace skity {

static std::vector<std::shared_ptr<Codec>> codec_list = {};

void Codec::SetupCodecs() {
  codec_list.clear();

  codec_list.emplace_back(std::make_shared<PNGCodec>());
  codec_list.emplace_back(std::make_shared<JPEGCodec>());
  codec_list.emplace_back(std::make_shared<GIFCodec>());
}

std::shared_ptr<Codec> Codec::MakeFromData(const std::shared_ptr<Data>& data) {
  if (codec_list.empty()) {
    SetupCodecs();
  }

  if (!data || data->Size() <= 20) {
    return nullptr;
  }

  const char* header = reinterpret_cast<const char*>(data->RawData());

  for (auto const& codec : codec_list) {
    if (codec->RecognizeFileType(header, data->Size())) {
      return codec;
    }
  }

  return nullptr;
}

std::shared_ptr<Codec> Codec::MakePngCodec() {
  return std::make_shared<PNGCodec>();
}

std::shared_ptr<Codec> Codec::MakeJPEGCodec() {
  return std::make_shared<JPEGCodec>();
}

std::shared_ptr<Codec> Codec::MakeGIFCodec() {
  return std::make_shared<GIFCodec>();
}

}  // namespace skity
