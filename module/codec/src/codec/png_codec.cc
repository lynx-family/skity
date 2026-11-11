// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/png_codec.hpp"

#include <cstdlib>
#include <cstring>
#include <skity/graphic/color_space.hpp>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <vector>

#include "src/codec/codec_priv.hpp"
#include "src/codec/core_bridge.hpp"

namespace skity {

#define PNG_BYTES_TO_CHECK 4

namespace {

constexpr size_t kPNGSignatureSize = 8;
// PNG stores gAMA / cHRM values scaled by this factor.
constexpr uint32_t kPNGFixedPointScale = 100000;

struct PngColorChunkInfo {
  bool has_srgb = false;
  bool has_gama = false;
  bool has_chrm = false;
  // gAMA / cHRM carry ICC-based color descriptions skity cannot reduce yet
  // (see docs/COLOR_SPACE_DESIGN.md); their presence falls back to sRGB.
  bool has_icc_description = false;
  uint32_t gama = 0;
  uint32_t chrm[8] = {};
};

uint32_t ReadBE32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
         (uint32_t(p[2]) << 8) | uint32_t(p[3]);
}

// Walks the PNG chunk stream up to the first IDAT and collects the color
// description chunks. CRCs are not validated here; the actual decode reads
// the same data through libpng, which rejects corrupt files anyway.
bool ParsePNGColorChunks(const uint8_t* data, size_t size,
                         PngColorChunkInfo* out) {
  static const uint8_t kPNGSignature[kPNGSignatureSize] = {
      0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
  if (size < kPNGSignatureSize ||
      std::memcmp(data, kPNGSignature, kPNGSignatureSize) != 0) {
    return false;
  }

  size_t pos = kPNGSignatureSize;
  while (pos + 12 <= size) {
    uint32_t length = ReadBE32(data + pos);
    const uint8_t* type = data + pos + 4;
    if (length > size - pos - 12) {
      break;  // truncated chunk table; stop scanning
    }
    const uint8_t* payload = data + pos + 8;

    if (std::memcmp(type, "IDAT", 4) == 0) {
      break;  // color chunks must precede the image data
    } else if (std::memcmp(type, "sRGB", 4) == 0 && length >= 1) {
      out->has_srgb = true;
    } else if (std::memcmp(type, "gAMA", 4) == 0 && length >= 4) {
      out->gama = ReadBE32(payload);
      out->has_gama = out->gama > 0 && out->gama < 10 * kPNGFixedPointScale;
    } else if (std::memcmp(type, "cHRM", 4) == 0 && length >= 32) {
      for (int i = 0; i < 8; i++) {
        out->chrm[i] = ReadBE32(payload + i * 4);
      }
      out->has_chrm = true;
    } else if (std::memcmp(type, "iCCP", 4) == 0 ||
               std::memcmp(type, "cICP", 4) == 0) {
      out->has_icc_description = true;
    }

    pos += 12 + length;
  }
  return true;
}

// Turns the collected chunks into a parametric color space description.
// Returns false when the file carries no (supported) color description and
// the pixmap should keep the default sRGB tag.
bool BuildColorSpaceInfo(const PngColorChunkInfo& chunks,
                         skity_color_space_info* out) {
  if (chunks.has_srgb || chunks.has_icc_description) {
    // sRGB overrides gAMA/cHRM per the PNG spec; ICC profiles cannot be
    // reduced to the parametric form yet, so both fall back to sRGB.
    return false;
  }
  if (!chunks.has_gama && !chunks.has_chrm) {
    return false;
  }

  ColorSpace srgb{};

  out->kind = SKITY_COLOR_SPACE_PARAMETRIC;
  // No gAMA: the PNG spec lets viewers assume sRGB-like encoding.
  std::memcpy(out->transfer_fn, srgb.transfer_fn, sizeof(out->transfer_fn));
  if (chunks.has_gama) {
    float gamma = static_cast<float>(chunks.gama) / kPNGFixedPointScale;
    float tf[7] = {gamma, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    std::memcpy(out->transfer_fn, tf, sizeof(tf));
  }

  std::memcpy(out->gamut_xyz, srgb.gamut_xyz, sizeof(out->gamut_xyz));
  if (chunks.has_chrm) {
    float chromaticities[8];
    for (int i = 0; i < 8; i++) {
      chromaticities[i] =
          static_cast<float>(chunks.chrm[i]) / kPNGFixedPointScale;
    }
    if (!codec_priv::CodecGamutFromChromaticities(chromaticities,
                                                  out->gamut_xyz)) {
      // Degenerate primaries: keep the sRGB gamut rather than failing.
      std::memcpy(out->gamut_xyz, srgb.gamut_xyz, sizeof(out->gamut_xyz));
    }
  }
  return true;
}

// Writes gAMA / cHRM chunks for a non-sRGB pixmap. Only descriptions the
// chunks can express exactly are written: a pure power gamma and any gamut.
void WritePNGColorSpaceChunks(png_structp png_ptr, png_infop info_ptr,
                              const Pixmap* pixmap) {
  skity_color_space_info info{};
  if (!core_bridge::GetPixmapColorSpace(const_cast<Pixmap*>(pixmap), &info)) {
    return;
  }
  if (info.kind == SKITY_COLOR_SPACE_SRGB) {
    return;
  }

  const float* tf = info.transfer_fn;
  bool pure_power = tf[1] == 1.f && tf[2] == 0.f && tf[3] == 0.f &&
                    tf[4] == 0.f && tf[5] == 0.f && tf[6] == 0.f &&
                    tf[0] > 0.f && tf[0] < 10.f;
  if (pure_power) {
    png_set_gAMA(png_ptr, info_ptr, tf[0]);
  }

  ColorSpace srgb{};
  if (std::memcmp(info.gamut_xyz, srgb.gamut_xyz, sizeof(info.gamut_xyz)) !=
      0) {
    // gamut_xyz is D50-adapted (see ColorSpace::gamut_xyz) while a cHRM chunk
    // stores the native primaries and white point, so the chunk values come
    // from the matrix with the adaptation undone (towards D65), not from its
    // columns directly.
    float c[8];
    if (codec_priv::CodecChromaticitiesFromGamut(info.gamut_xyz, c)) {
      png_set_cHRM(png_ptr, info_ptr, c[0], c[1], c[2], c[3], c[4], c[5], c[6],
                   c[7]);
    }
  }
}

}  // namespace

static void png_write_callback(png_structp png_ptr, png_bytep data,
                               png_size_t length) {
  auto encoded_data =
      reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png_ptr));

  for (size_t i = 0; i < length; i++) {
    encoded_data->emplace_back(data[i]);
  }
}

struct PNGImage {
  png_image image = {};

  PNGImage() : image() { image.version = PNG_IMAGE_VERSION; }

  ~PNGImage() { png_image_free(&image); }
};

PNGCodec::PNGCodec() = default;

PNGCodec::~PNGCodec() {}

std::shared_ptr<Pixmap> skity::PNGCodec::Decode(const DecodeOptions& options) {
  // libpng has no scaled decoding; decode at intrinsic size and resample.
  return codec_priv::ResamplePixmapToTarget(DecodeIntrinsic(), options);
}

std::shared_ptr<Pixmap> skity::PNGCodec::DecodeIntrinsic() {
  if (!data_) {
    return nullptr;
  }

  PNGImage png_image{};
  if (!png_image_begin_read_from_memory(&png_image.image, data_->RawData(),
                                        data_->Size())) {
    return nullptr;
  }

  png_bytep buffer;
  png_image.image.format = PNG_FORMAT_RGBA;
  size_t raw_data_size = PNG_IMAGE_SIZE(png_image.image);
  buffer = static_cast<png_bytep>(std::malloc(raw_data_size));
  if (!buffer) {
    // out of memory
    return nullptr;
  }

  if (!png_image_finish_read(&png_image.image, nullptr, buffer, 0, nullptr)) {
    std::free(buffer);
    return nullptr;
  }

  auto raw_data = core_bridge::MakeDataFromMalloc(buffer, raw_data_size);
  uint32_t width = png_image.image.width;
  uint32_t height = png_image.image.height;

  auto pixmap =
      core_bridge::MakePixmap(raw_data, width * 4, width, height,
                              AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);
  if (pixmap) {
    PngColorChunkInfo chunks{};
    skity_color_space_info info{};
    if (ParsePNGColorChunks(static_cast<const uint8_t*>(data_->RawData()),
                            data_->Size(), &chunks) &&
        BuildColorSpaceInfo(chunks, &info)) {
      core_bridge::SetPixmapColorSpace(pixmap.get(), info);
    }
  }
  return pixmap;
}

std::shared_ptr<MultiFrameDecoder> PNGCodec::DecodeMultiFrame() { return {}; }

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

  WritePNGColorSpaceChunks(png_ptr, info_ptr, pixmap);

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

  return core_bridge::MakeDataWithCopy(encode_data.data(), encode_data.size());
}

bool skity::PNGCodec::RecognizeFileType(const char* header, size_t size) {
  return !png_sig_cmp((png_const_bytep)header, (png_size_t)0,
                      PNG_BYTES_TO_CHECK);
}

}  // namespace skity
