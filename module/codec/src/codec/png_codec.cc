// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/codec/png_codec.hpp"

#include <csetjmp>
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

// PNG stores gAMA / cHRM values scaled by this factor.
constexpr uint32_t kPNGFixedPointScale = 100000;

// Cursor feeding libpng from the source data; libpng pulls bytes from it
// through the read callback below.
struct PngReadSource {
  const uint8_t* data;
  size_t size;
  size_t pos;
};

void png_read_callback(png_structp png_ptr, png_bytep out, png_size_t length) {
  auto* source = static_cast<PngReadSource*>(png_get_io_ptr(png_ptr));
  if (length > source->size - source->pos) {
    png_error(png_ptr, "unexpected end of PNG data");
    return;
  }
  std::memcpy(out, source->data + source->pos, length);
  source->pos += length;
}

// Turns libpng's color description chunks into a parametric color space
// description. Reading them from the info struct (instead of rescanning the
// byte stream) keeps metadata and pixels under one set of rules: libpng
// validates every chunk CRC and discards ancillary chunks (gAMA, cHRM, ...)
// with a bad one, and it already ran its checks on the data it decoded.
// Returns false when the file carries no (supported) color description and
// the pixmap should keep the default sRGB tag.
bool BuildColorSpaceInfo(png_structp png_ptr, png_infop info_ptr,
                         skity_color_space_info* out) {
  // sRGB overrides gAMA/cHRM per the PNG spec; ICC profiles cannot be
  // reduced to the parametric form yet, so both fall back to sRGB.
  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_sRGB) != 0) {
    return false;
  }
  {
    png_charp profile_name = nullptr;
    int compression = 0;
    png_bytep profile = nullptr;
    png_uint_32 profile_len = 0;
    if (png_get_iCCP(png_ptr, info_ptr, &profile_name, &compression, &profile,
                     &profile_len) != 0) {
      return false;
    }
  }

  png_fixed_point gama_fixed = 0;
  png_fixed_point chrm_fixed[8] = {};
  bool has_gama = png_get_gAMA_fixed(png_ptr, info_ptr, &gama_fixed) != 0;
  bool has_chrm =
      png_get_cHRM_fixed(png_ptr, info_ptr, &chrm_fixed[0], &chrm_fixed[1],
                         &chrm_fixed[2], &chrm_fixed[3], &chrm_fixed[4],
                         &chrm_fixed[5], &chrm_fixed[6], &chrm_fixed[7]) != 0;
  if (has_gama &&
      (gama_fixed <= 0 || gama_fixed >= 10 * (int32_t)kPNGFixedPointScale)) {
    has_gama = false;
  }
  if (!has_gama && !has_chrm) {
    return false;
  }

  ColorSpace srgb{};

  out->kind = SKITY_COLOR_SPACE_PARAMETRIC;
  // No gAMA: the PNG spec lets viewers assume sRGB-like encoding.
  std::memcpy(out->transfer_fn, srgb.transfer_fn, sizeof(out->transfer_fn));
  if (has_gama) {
    // gAMA stores the encoding gamma (45455 means 1/2.2) while transfer_fn
    // holds the decode (signal -> linear) exponent, so the reciprocal is
    // what lands in the tag.
    float encode_gamma = static_cast<float>(gama_fixed) / kPNGFixedPointScale;
    float tf[7] = {1.f / encode_gamma, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    std::memcpy(out->transfer_fn, tf, sizeof(tf));
  }

  std::memcpy(out->gamut_xyz, srgb.gamut_xyz, sizeof(out->gamut_xyz));
  if (has_chrm) {
    float chromaticities[8];
    for (int i = 0; i < 8; i++) {
      chromaticities[i] =
          static_cast<float>(chrm_fixed[i]) / kPNGFixedPointScale;
    }
    if (!ColorSpace::GamutFromChromaticities(chromaticities, out->gamut_xyz)) {
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
    // transfer_fn[0] is the decode (signal -> linear) exponent while gAMA
    // stores the encoding gamma, so the chunk carries the reciprocal.
    png_set_gAMA(png_ptr, info_ptr, 1.f / tf[0]);
  }

  ColorSpace srgb{};
  if (std::memcmp(info.gamut_xyz, srgb.gamut_xyz, sizeof(info.gamut_xyz)) !=
      0) {
    // gamut_xyz is D50-adapted (see ColorSpace::gamut_xyz) while a cHRM chunk
    // stores the native primaries and white point, so the chunk values come
    // from the matrix with the adaptation undone (towards D65), not from its
    // columns directly.
    float c[8];
    if (ColorSpace::ChromaticitiesFromGamut(info.gamut_xyz, c)) {
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

  png_structp png_read_ptr =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if (!png_read_ptr) {
    return nullptr;
  }
  png_infop read_info_ptr = png_create_info_struct(png_read_ptr);
  if (!read_info_ptr) {
    png_destroy_read_struct(&png_read_ptr, nullptr, nullptr);
    return nullptr;
  }

  // A png_error longjmp below lands back here. C++ destructors are skipped
  // by longjmp, so the raw pointers must be released by hand on that path.
  uint8_t* buffer = nullptr;
  png_bytep* rows = nullptr;

  if (setjmp(png_jmpbuf(png_read_ptr)) != 0) {
    std::free(buffer);
    std::free(rows);
    png_destroy_read_struct(&png_read_ptr, &read_info_ptr, nullptr);
    return nullptr;
  }

  PngReadSource source{static_cast<const uint8_t*>(data_->RawData()),
                       data_->Size(), 0};
  png_set_read_fn(png_read_ptr, &source, png_read_callback);

  // The low-level reader decodes the file's own encoding into the row
  // buffers: no gamma transform is requested, so the output bytes are what
  // the color description chunks describe and the conversion happens later,
  // at sampling time. (The simplified png_image API would have handed out
  // sRGB-encoded pixels instead, double-converting once tagged.)
  png_read_info(png_read_ptr, read_info_ptr);

  png_uint_32 width = png_get_image_width(png_read_ptr, read_info_ptr);
  png_uint_32 height = png_get_image_height(png_read_ptr, read_info_ptr);
  png_uint_32 color_type = png_get_color_type(png_read_ptr, read_info_ptr);
  png_uint_32 bit_depth = png_get_bit_depth(png_read_ptr, read_info_ptr);

  // Normalize every input layout to 8-bit RGBA.
  if (color_type == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(png_read_ptr);
  }
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
    png_set_expand_gray_1_2_4_to_8(png_read_ptr);
  }
  if (png_get_valid(png_read_ptr, read_info_ptr, PNG_INFO_tRNS) != 0) {
    png_set_tRNS_to_alpha(png_read_ptr);
  }
  if (bit_depth == 16) {
    png_set_strip_16(png_read_ptr);
  }
  if ((color_type & PNG_COLOR_MASK_COLOR) == 0) {
    png_set_gray_to_rgb(png_read_ptr);
  }
  bool has_alpha =
      (color_type & PNG_COLOR_MASK_ALPHA) != 0 ||
      png_get_valid(png_read_ptr, read_info_ptr, PNG_INFO_tRNS) != 0;
  if (!has_alpha) {
    png_set_filler(png_read_ptr, 0xff, PNG_FILLER_AFTER);
  }

  png_set_interlace_handling(png_read_ptr);
  png_read_update_info(png_read_ptr, read_info_ptr);

  size_t row_bytes = png_get_rowbytes(png_read_ptr, read_info_ptr);
  size_t raw_data_size = row_bytes * height;
  buffer = static_cast<uint8_t*>(std::malloc(raw_data_size));
  rows = static_cast<png_bytep*>(std::malloc(sizeof(png_bytep) * height));
  if (!buffer || !rows) {
    // out of memory
    std::free(buffer);
    std::free(rows);
    png_destroy_read_struct(&png_read_ptr, &read_info_ptr, nullptr);
    return nullptr;
  }
  for (png_uint_32 y = 0; y < height; y++) {
    rows[y] = buffer + static_cast<size_t>(y) * row_bytes;
  }
  // png_read_image walks all interlace passes into the packed rows.
  png_read_image(png_read_ptr, rows);
  png_read_end(png_read_ptr, nullptr);
  std::free(rows);

  auto raw_data = core_bridge::MakeDataFromMalloc(buffer, raw_data_size);

  auto pixmap =
      core_bridge::MakePixmap(raw_data, row_bytes, width, height,
                              AlphaType::kUnpremul_AlphaType, ColorType::kRGBA);
  if (pixmap) {
    skity_color_space_info info{};
    if (BuildColorSpaceInfo(png_read_ptr, read_info_ptr, &info)) {
      core_bridge::SetPixmapColorSpace(pixmap.get(), info);
    }
  }
  png_destroy_read_struct(&png_read_ptr, &read_info_ptr, nullptr);
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
