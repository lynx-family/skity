// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/jpeg_codec.hpp"

extern "C" {
#include <stdio.h>
// After stdio.h
#include <jpeglib.h>
}

#include <turbojpeg.h>

#include <array>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>

#include "src/codec/codec_priv.hpp"

namespace skity {

namespace {

struct TJHandlerWrapper {
  explicit TJHandlerWrapper(tjhandle h) : handle(h) {}

  ~TJHandlerWrapper() {
    if (this->handle) {
      tjDestroy(this->handle);
    }
  }

  tjhandle handle = nullptr;
};

void init_jpeg_destination(j_compress_ptr cinfo);
boolean empty_jpeg_output_buffer(j_compress_ptr cinfo);
void term_jpeg_destination(j_compress_ptr cinfo);

struct skity_jpeg_destination : jpeg_destination_mgr {
  skity_jpeg_destination() {
    this->init_destination = init_jpeg_destination;
    this->empty_output_buffer = empty_jpeg_output_buffer;
    this->term_destination = term_jpeg_destination;
  }

  std::vector<uint8_t> data{};

  std::array<uint8_t, 1024> buffer{};
};

void init_jpeg_destination(j_compress_ptr cinfo) {
  auto dest = reinterpret_cast<skity_jpeg_destination*>(cinfo->dest);

  dest->next_output_byte = dest->buffer.data();
  dest->free_in_buffer = dest->buffer.size();
}

boolean empty_jpeg_output_buffer(j_compress_ptr cinfo) {
  auto dest = reinterpret_cast<skity_jpeg_destination*>(cinfo->dest);
  dest->data.insert(dest->data.end(), dest->buffer.begin(), dest->buffer.end());

  dest->next_output_byte = dest->buffer.data();
  dest->free_in_buffer = dest->buffer.size();

  return true;
}

void term_jpeg_destination(j_compress_ptr cinfo) {
  auto dest = reinterpret_cast<skity_jpeg_destination*>(cinfo->dest);

  auto size = dest->buffer.size() - dest->free_in_buffer;

  if (size > 0) {
    dest->data.insert(dest->data.end(), dest->buffer.begin(),
                      dest->buffer.begin() + size);
  }
}

}  // namespace

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

std::shared_ptr<MultiFrameDecoder> JPEGCodec::DecodeMultiFrame() { return {}; }

std::shared_ptr<Data> JPEGCodec::Encode(const Pixmap* pixmap) {
  if (!pixmap || pixmap->Width() == 0 || pixmap->Height() == 0) {
    return nullptr;
  }

  jpeg_error_mgr jerr{};

  jpeg_compress_struct cinfo{};
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);

  skity_jpeg_destination dest{};
  cinfo.dest = &dest;

  cinfo.image_width = pixmap->Width();
  cinfo.image_height = pixmap->Height();

  cinfo.in_color_space = JCS_EXT_RGBA;
  if (pixmap->GetColorType() == ColorType::kBGRA) {
    cinfo.in_color_space = JCS_EXT_BGRA;
  }
  cinfo.input_components = 4;
  jpeg_set_defaults(&cinfo);
  jpeg_set_colorspace(&cinfo, JCS_RGB);

  cinfo.optimize_coding = TRUE;
  // 100 is the highest quality
  jpeg_set_quality(&cinfo, 100, TRUE);
  jpeg_start_compress(&cinfo, TRUE);

  // jpeg can do swizzel, we only needs to convert alpha type to unpremul
  codec_priv::TransformLineFunc transform_func =
      codec_priv::CodecTransformLineByPass;

  // jpeg does not have alpha channel, if alpha type is unpremul, we need to
  // convert it to premul.
  if (pixmap->GetAlphaType() == AlphaType::kUnpremul_AlphaType) {
    transform_func = codec_priv::CodecTransformLinePremul;
  }

  std::vector<uint8_t*> bytepp(pixmap->Height());
  for (size_t i = 0; i < bytepp.size(); i++) {
    bytepp[i] = ((uint8_t*)pixmap->Addr()) + pixmap->Width() * i * 4;  // NOLINT
  }

  auto bytes_per_pixel = pixmap->RowBytes() / pixmap->Width();
  for (int y = 0; y < pixmap->Height(); y++) {
    std::vector<uint8_t> row(pixmap->RowBytes());

    transform_func(row.data(), bytepp[y], pixmap->Width(), bytes_per_pixel);

    auto row_data_ptr = row.data();
    jpeg_write_scanlines(&cinfo, &row_data_ptr, 1);
  }

  jpeg_finish_compress(&cinfo);

  jpeg_destroy_compress(&cinfo);

  return skity::Data::MakeWithCopy(dest.data.data(), dest.data.size());
}

}  // namespace skity
