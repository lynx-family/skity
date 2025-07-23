// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/jpeg_codec.hpp"

#include <turbojpeg.h>

#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>

struct TJHandlerWrapper {
  explicit TJHandlerWrapper(tjhandle h) : handle(h) {}

  ~TJHandlerWrapper() {
    if (this->handle) {
      tjDestroy(this->handle);
    }
  }

  tjhandle handle = nullptr;
};

namespace skity {

bool JPEGCodec::RecognizeFileType(const char* header, size_t size) {
  TJHandlerWrapper hw{tjInitDecompress()};

  if (!hw.handle) {
    // JPEG init failed
    return false;
  }

  int32_t width;
  int32_t height;

  int ret = tjDecompressHeader(hw.handle, (unsigned char*)header, size, &width,
                               &height);
  if (ret == 0) {
    return true;
  } else {
    return false;
  }
}

std::shared_ptr<Pixmap> JPEGCodec::Decode() {
  TJHandlerWrapper hw{tjInitDecompress()};

  if (!hw.handle) {
    // JPEG init failed
    return nullptr;
  }

  int32_t width;
  int32_t height;

  int ret = tjDecompressHeader(hw.handle, (unsigned char*)data_->RawData(),
                               data_->Size(), &width, &height);

  if (ret != 0) {
    return nullptr;
  }

  uint8_t* buffer = reinterpret_cast<uint8_t*>(
      tjAlloc(width * height * tjPixelSize[TJPF_RGBA]));

  ret = tjDecompress2(hw.handle, (const unsigned char*)data_->RawData(),
                      data_->Size(), (unsigned char*)buffer, width, 0, height,
                      TJPF_RGBA, 0);
  if (ret != 0) {
    tjFree(buffer);
    return nullptr;
  }

  auto image_data = skity::Data::MakeWithCopy(
      buffer, width * height * tjPixelSize[TJPF_RGBA]);

  tjFree(buffer);
  return std::make_shared<Pixmap>(image_data, width * tjPixelSize[TJPF_RGBA],
                                  width, height);
}

std::shared_ptr<Data> JPEGCodec::Encode(const Pixmap* pixmap) {
  TJHandlerWrapper hw{tjInitCompress()};

  uint8_t* buf = nullptr;
  unsigned long size = 0;  // NOLINT

  int32_t ret =
      tjCompress2(hw.handle, (const uint8_t*)pixmap->Addr(), pixmap->Width(),
                  pixmap->Width() * 4, pixmap->Height(), TJPF_RGBA, &buf, &size,
                  TJSAMP_444, 100, 0);

  if (ret != 0) {
    return nullptr;
  }

  return Data::MakeWithCopy(buf, size);
}

}  // namespace skity
