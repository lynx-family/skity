// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/bmp_codec.hpp"

#include <cstdlib>
#include <cstring>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <vector>

namespace skity {

namespace {

enum BMPCompression {
  BI_RGB = 0,
  BI_BITFIELDS = 3,
};

inline uint16_t ReadU16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

inline uint32_t ReadU32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

inline int32_t ReadI32(const uint8_t* data) {
  return static_cast<int32_t>(ReadU32(data));
}

inline void WriteU16(std::vector<uint8_t>& buf, uint16_t val) {
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
}

inline void WriteU32(std::vector<uint8_t>& buf, uint32_t val) {
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
}

inline void WriteI32(std::vector<uint8_t>& buf, int32_t val) {
  WriteU32(buf, static_cast<uint32_t>(val));
}

uint8_t ExtractComponent(uint32_t val, uint32_t mask) {
  if (mask == 0) {
    return 0;
  }
  uint32_t v = val & mask;
  uint32_t m = mask;
  while ((m & 1) == 0) {
    v >>= 1;
    m >>= 1;
  }
  uint32_t bits = 0;
  uint32_t tmp = m;
  while (tmp) {
    bits++;
    tmp >>= 1;
  }
  if (bits >= 8) {
    return static_cast<uint8_t>(v & 0xFF);
  }
  uint8_t result = static_cast<uint8_t>(v);
  for (uint32_t shift = bits; shift < 8; shift += bits) {
    result = static_cast<uint8_t>(result | (result >> bits));
  }
  return result;
}

}  // namespace

bool BMPCodec::RecognizeFileType(const char* header, size_t size) {
  return size >= 2 && header[0] == 'B' && header[1] == 'M';
}

std::shared_ptr<MultiFrameDecoder> BMPCodec::DecodeMultiFrame() { return {}; }

std::shared_ptr<Pixmap> BMPCodec::Decode() {
  const auto* raw = static_cast<const uint8_t*>(data_->RawData());
  size_t size = data_->Size();

  if (size < 54) {
    return nullptr;
  }

  uint16_t signature = ReadU16(raw);
  if (signature != 0x4D42) {
    return nullptr;
  }
  uint32_t data_offset = ReadU32(raw + 10);

  size_t offset = 14;
  uint32_t header_size = ReadU32(raw + offset);
  if (header_size < 40) {
    return nullptr;
  }

  int32_t width_abs = ReadI32(raw + offset + 4);
  int32_t height_abs = ReadI32(raw + offset + 8);
  uint16_t bpp = ReadU16(raw + offset + 14);
  uint32_t compression = ReadU32(raw + offset + 16);
  uint32_t colors_used = ReadU32(raw + offset + 32);

  uint32_t width = static_cast<uint32_t>(std::abs(width_abs));
  uint32_t height = static_cast<uint32_t>(std::abs(height_abs));
  bool top_down = height_abs < 0;

  if (width == 0 || height == 0) {
    return nullptr;
  }

  if (compression != BI_RGB && compression != BI_BITFIELDS) {
    return nullptr;
  }
  if (bpp != 8 && bpp != 24 && bpp != 32) {
    return nullptr;
  }
  if (compression == BI_BITFIELDS && bpp != 32) {
    return nullptr;
  }

  uint32_t r_mask = 0x00FF0000;
  uint32_t g_mask = 0x0000FF00;
  uint32_t b_mask = 0x000000FF;
  uint32_t a_mask = 0xFF000000;

  size_t palette_offset = offset + header_size;

  if (compression == BI_BITFIELDS) {
    if (palette_offset + 16 > size) {
      return nullptr;
    }
    r_mask = ReadU32(raw + palette_offset);
    g_mask = ReadU32(raw + palette_offset + 4);
    b_mask = ReadU32(raw + palette_offset + 8);
    a_mask = ReadU32(raw + palette_offset + 12);
    palette_offset += 16;
  }

  std::vector<uint8_t> palette;
  if (bpp == 8) {
    uint32_t entries = (colors_used == 0) ? 256 : colors_used;
    palette.resize(entries * 4);
    for (uint32_t i = 0; i < entries; i++) {
      if (palette_offset + 4 > size) {
        return nullptr;
      }
      palette[i * 4 + 0] = raw[palette_offset + 2];
      palette[i * 4 + 1] = raw[palette_offset + 1];
      palette[i * 4 + 2] = raw[palette_offset + 0];
      palette[i * 4 + 3] = 0xFF;
      palette_offset += 4;
    }
  }

  uint32_t row_size = ((width * bpp + 31) / 32) * 4;

  auto* output = static_cast<uint8_t*>(std::malloc(width * height * 4));
  if (!output) {
    return nullptr;
  }

  const uint8_t* pixel_data = raw + data_offset;

  bool all_alpha_zero = false;
  if (bpp == 32 && compression == BI_RGB) {
    all_alpha_zero = true;
    for (uint32_t yy = 0; yy < height && all_alpha_zero; yy++) {
      uint32_t src_y_check = top_down ? yy : (height - 1 - yy);
      const uint8_t* src_row_check = pixel_data + src_y_check * row_size;
      for (uint32_t xx = 0; xx < width && all_alpha_zero; xx++) {
        if (src_row_check[xx * 4 + 3] != 0) {
          all_alpha_zero = false;
        }
      }
    }
  }

  for (uint32_t y = 0; y < height; y++) {
    uint32_t src_y = top_down ? y : (height - 1 - y);
    const uint8_t* src_row = pixel_data + src_y * row_size;
    uint8_t* dst_row = output + y * width * 4;

    if (bpp == 8) {
      for (uint32_t x = 0; x < width; x++) {
        uint8_t idx = src_row[x] * 4;
        dst_row[x * 4 + 0] = palette[idx + 0];
        dst_row[x * 4 + 1] = palette[idx + 1];
        dst_row[x * 4 + 2] = palette[idx + 2];
        dst_row[x * 4 + 3] = palette[idx + 3];
      }
    } else if (bpp == 24) {
      for (uint32_t x = 0; x < width; x++) {
        dst_row[x * 4 + 0] = src_row[x * 3 + 2];
        dst_row[x * 4 + 1] = src_row[x * 3 + 1];
        dst_row[x * 4 + 2] = src_row[x * 3 + 0];
        dst_row[x * 4 + 3] = 0xFF;
      }
    } else {
      if (compression == BI_RGB) {
        for (uint32_t x = 0; x < width; x++) {
          dst_row[x * 4 + 0] = src_row[x * 4 + 2];
          dst_row[x * 4 + 1] = src_row[x * 4 + 1];
          dst_row[x * 4 + 2] = src_row[x * 4 + 0];
          uint8_t alpha = src_row[x * 4 + 3];
          dst_row[x * 4 + 3] = all_alpha_zero ? 0xFF : alpha;
        }
      } else {
        for (uint32_t x = 0; x < width; x++) {
          uint32_t pixel = ReadU32(src_row + x * 4);
          dst_row[x * 4 + 0] = ExtractComponent(pixel, r_mask);
          dst_row[x * 4 + 1] = ExtractComponent(pixel, g_mask);
          dst_row[x * 4 + 2] = ExtractComponent(pixel, b_mask);
          dst_row[x * 4 + 3] = a_mask ? ExtractComponent(pixel, a_mask) : 0xFF;
        }
      }
    }
  }

  auto raw_data = Data::MakeFromMalloc(output, width * height * 4);
  return std::make_shared<Pixmap>(raw_data, width * 4, width, height);
}

std::shared_ptr<Data> BMPCodec::Encode(const Pixmap* pixmap) {
  if (!pixmap || pixmap->Width() == 0 || pixmap->Height() == 0) {
    return nullptr;
  }

  uint32_t width = pixmap->Width();
  uint32_t height = pixmap->Height();
  bool is_bgra = pixmap->GetColorType() == ColorType::kBGRA;

  bool has_alpha = pixmap->GetAlphaType() != AlphaType::kOpaque_AlphaType;
  uint16_t bpp = has_alpha ? 32 : 24;
  uint32_t row_size = ((width * bpp + 31) / 32) * 4;
  uint32_t pixel_data_size = row_size * height;
  uint32_t data_offset = 54;
  uint32_t file_size = data_offset + pixel_data_size;

  std::vector<uint8_t> output;
  output.reserve(file_size);

  WriteU16(output, 0x4D42);
  WriteU32(output, file_size);
  WriteU16(output, 0);
  WriteU16(output, 0);
  WriteU32(output, data_offset);

  WriteU32(output, 40);
  WriteI32(output, static_cast<int32_t>(width));
  WriteI32(output, static_cast<int32_t>(height));
  WriteU16(output, 1);
  WriteU16(output, bpp);
  WriteU32(output, BI_RGB);
  WriteU32(output, pixel_data_size);
  WriteI32(output, 2835);
  WriteI32(output, 2835);
  WriteU32(output, 0);
  WriteU32(output, 0);

  const auto* src = static_cast<const uint8_t*>(pixmap->Addr());
  uint32_t src_row_bytes = static_cast<uint32_t>(pixmap->RowBytes());

  for (int32_t y = static_cast<int32_t>(height) - 1; y >= 0; y--) {
    const uint8_t* src_row = src + y * src_row_bytes;

    if (bpp == 24) {
      for (uint32_t x = 0; x < width; x++) {
        if (is_bgra) {
          output.push_back(src_row[x * 4 + 0]);
          output.push_back(src_row[x * 4 + 1]);
          output.push_back(src_row[x * 4 + 2]);
        } else {
          output.push_back(src_row[x * 4 + 2]);
          output.push_back(src_row[x * 4 + 1]);
          output.push_back(src_row[x * 4 + 0]);
        }
      }
      for (uint32_t p = width * 3; p < row_size; p++) {
        output.push_back(0);
      }
    } else {
      for (uint32_t x = 0; x < width; x++) {
        if (is_bgra) {
          output.push_back(src_row[x * 4 + 0]);
          output.push_back(src_row[x * 4 + 1]);
          output.push_back(src_row[x * 4 + 2]);
          output.push_back(src_row[x * 4 + 3]);
        } else {
          output.push_back(src_row[x * 4 + 2]);
          output.push_back(src_row[x * 4 + 1]);
          output.push_back(src_row[x * 4 + 0]);
          output.push_back(src_row[x * 4 + 3]);
        }
      }
    }
  }

  return Data::MakeWithCopy(output.data(), output.size());
}

}  // namespace skity
