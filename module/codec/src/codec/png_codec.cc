// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/png_codec.hpp"

#include <cstdlib>
#include <cstring>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <vector>

#include "src/codec/codec_priv.hpp"

namespace skity {

#define PNG_BYTES_TO_CHECK 4

static void png_write_callback(png_structp png_ptr, png_bytep data,
                               png_size_t length) {
  auto encoded_data =
      reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));

  for (size_t i = 0; i < length; i++) {
    encoded_data->emplace_back(data[i]);
  }
}

PNGCodec::PNGCodec() = default;

PNGCodec::~PNGCodec() { png_image_free(&image_); }

std::shared_ptr<Pixmap> skity::PNGCodec::Decode() {
  if (image_.opaque) {
    png_image_free(&image_);
    image_.opaque = nullptr;
  }
  image_.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_memory(&image_, data_->RawData(),
                                        data_->Size())) {
    return nullptr;
  }

  png_bytep buffer;
  image_.format = PNG_FORMAT_RGBA;
  size_t raw_data_size = PNG_IMAGE_SIZE(image_);
  buffer = static_cast<png_bytep>(std::malloc(raw_data_size));
  if (!buffer) {
    // out of memory
    png_image_free(&image_);
    return nullptr;
  }

  if (!png_image_finish_read(&image_, nullptr, buffer, 0, nullptr)) {
    std::free(buffer);
    return nullptr;
  }

  auto raw_data = Data::MakeFromMalloc(buffer, raw_data_size);
  uint32_t width = image_.width;
  uint32_t height = image_.height;

  pixmap_ = std::make_shared<Pixmap>(raw_data, width * 4, width, height);

  return pixmap_;
}

struct PNGDestructor {
  png_structp p;
  explicit PNGDestructor(png_structp p) : p(p) {}
  ~PNGDestructor() {
    if (p) {
      png_destroy_write_struct(&p, nullptr);
    }
  }
};

std::shared_ptr<Data> skity::PNGCodec::Encode(const Pixmap* pixmap) {
  if (!pixmap) {
    return nullptr;
  }

  png_structp png_ptr;
  png_infop info_ptr;

  png_ptr =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_ptr) {
    return nullptr;
  }

  PNGDestructor png_destructor{png_ptr};

  info_ptr = png_create_info_struct(png_ptr);
  png_set_IHDR(png_ptr, info_ptr, pixmap->Width(), pixmap->Height(), 8,
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

  std::vector<uint8_t*> bytepp(pixmap->Height());
  for (size_t i = 0; i < bytepp.size(); i++) {
    bytepp[i] = ((uint8_t*)pixmap->Addr()) + pixmap->Width() * i * 4;  // NOLINT
  }

  std::vector<uint8_t> encode_data{};
  png_set_write_fn(png_ptr, &encode_data, png_write_callback, nullptr);

  png_write_info(png_ptr, info_ptr);

  auto bytes_per_pixel = pixmap->RowBytes() / pixmap->Width();
  for (int y = 0; y < pixmap->Height(); y++) {
    std::vector<uint8_t> row(pixmap->RowBytes());

    auto transform_line = codec_priv::ChooseLineTransformFunc(
        pixmap->GetColorType(), pixmap->GetAlphaType());

    transform_line(row.data(), bytepp[y], pixmap->Width(), bytes_per_pixel);

    png_write_row(png_ptr, row.data());
  }

  png_write_end(png_ptr, info_ptr);

  return Data::MakeWithCopy(encode_data.data(), encode_data.size());
}

bool skity::PNGCodec::RecognizeFileType(const char* header, size_t size) {
  return !png_sig_cmp((png_const_bytep)header, (png_size_t)0,
                      PNG_BYTES_TO_CHECK);
}

}  // namespace skity
