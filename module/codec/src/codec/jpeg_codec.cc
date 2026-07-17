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
#include <csetjmp>
#include <cstring>
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

// Custom libjpeg error manager used by Decode() to recover from fatal errors
// (e.g. truncated or corrupted scan data) via setjmp/longjmp, so the
// already-decoded scanlines can be returned as a partial result instead of
// discarding the whole image.
struct JpegErrorMgr {
  jpeg_error_mgr base;
  jmp_buf setjmp_buffer;
};

void JpegErrorExit(j_common_ptr cinfo) {
  auto* mgr = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
  // Jump back to Decode(); it returns whatever was decoded so far.
  longjmp(mgr->setjmp_buffer, 1);
}

void JpegOutputMessage(j_common_ptr cinfo) {
  // Swallow libjpeg warning/error text. Corrupted images can emit many
  // warnings; keep the library silent.
}

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

// Memory-backed source manager — a drop-in replacement for jpeg_mem_src().
// jpeg_mem_src() is only declared in libjpeg headers >= v8 (and in
// libjpeg-turbo), so depending on it breaks builds against legacy IJG libjpeg
// v6b/v7. The jpeg_source_mgr callbacks used here have existed since libjpeg
// v6b, so this works everywhere. Mirrors skity_jpeg_destination above.
void init_jpeg_source(j_decompress_ptr cinfo);
boolean fill_jpeg_input_buffer(j_decompress_ptr cinfo);
// num_bytes is `long` because that is the signature libjpeg requires for the
// skip_input_data callback (jpeg_source_mgr); it cannot be widened to int64_t.
void skip_jpeg_input_data(j_decompress_ptr cinfo,
                          long num_bytes);  // NOLINT(runtime/int)
void term_jpeg_source(j_decompress_ptr cinfo);

struct skity_jpeg_source : jpeg_source_mgr {
  skity_jpeg_source(const unsigned char* data, size_t size)
      : data_(data), size_(size) {
    init_source = init_jpeg_source;
    fill_input_buffer = fill_jpeg_input_buffer;
    skip_input_data = skip_jpeg_input_data;
    resync_to_restart = jpeg_resync_to_restart;
    term_source = term_jpeg_source;
    next_input_byte = nullptr;
    bytes_in_buffer = 0;
  }

  const unsigned char* data_;
  size_t size_;
  const unsigned char eoi_[2] = {0xFF, 0xD9};
};

void init_jpeg_source(j_decompress_ptr cinfo) {
  auto* src = reinterpret_cast<skity_jpeg_source*>(cinfo->src);
  // Hand the whole in-memory buffer to libjpeg up front, like jpeg_mem_src.
  src->next_input_byte = src->data_;
  src->bytes_in_buffer = src->size_;
}

boolean fill_jpeg_input_buffer(j_decompress_ptr cinfo) {
  auto* src = reinterpret_cast<skity_jpeg_source*>(cinfo->src);
  // Reached only once the buffer is exhausted (e.g. truncated stream): feed a
  // synthetic EOI so libjpeg finalizes gracefully and keeps what was decoded.
  src->next_input_byte = src->eoi_;
  src->bytes_in_buffer = sizeof(src->eoi_);
  return TRUE;
}

void skip_jpeg_input_data(j_decompress_ptr cinfo,
                          long num_bytes) {  // NOLINT(runtime/int)
  auto* src = reinterpret_cast<skity_jpeg_source*>(cinfo->src);
  if (num_bytes <= 0) {
    return;
  }
  while (num_bytes >
         static_cast<long>(src->bytes_in_buffer)) {  // NOLINT(runtime/int)
    num_bytes -=
        static_cast<long>(src->bytes_in_buffer);  // NOLINT(runtime/int)
    fill_jpeg_input_buffer(cinfo);
  }
  src->bytes_in_buffer -= static_cast<size_t>(num_bytes);
  src->next_input_byte += static_cast<size_t>(num_bytes);
}

void term_jpeg_source(j_decompress_ptr cinfo) { (void)cinfo; }

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
  if (!data_ || data_->Size() == 0) {
    return nullptr;
  }

  jpeg_decompress_struct cinfo;
  JpegErrorMgr jerr;
  cinfo.err = jpeg_std_error(&jerr.base);
  jerr.base.error_exit = JpegErrorExit;
  jerr.base.output_message = JpegOutputMessage;

  // Declared volatile: these are modified after setjmp() and read again after a
  // potential longjmp(); volatile guarantees the post-setjmp values survive the
  // non-local jump on every compiler.
  uint8_t* volatile pixels = nullptr;
  uint32_t volatile width = 0;
  uint32_t volatile height = 0;

  if (setjmp(jerr.setjmp_buffer)) {
    // A fatal libjpeg error occurred (e.g. corrupt/truncated scan data). Tear
    // down the decompressor and, if the output buffer was already allocated,
    // return what was decoded so far instead of failing — matching the
    // partial-decode behavior of Skia/browsers on broken JPEGs.
    jpeg_destroy_decompress(&cinfo);

    uint8_t* buf = pixels;
    if (buf && width > 0 && height > 0) {
      size_t size =
          static_cast<size_t>(width) * height * tjPixelSize[TJPF_RGBA];
      auto image_data = skity::Data::MakeWithCopy(buf, size);
      tjFree(buf);
      return std::make_shared<Pixmap>(
          image_data, static_cast<size_t>(width) * tjPixelSize[TJPF_RGBA],
          width, height);
    }
    if (buf) {
      tjFree(buf);
    }
    return nullptr;
  }

  jpeg_create_decompress(&cinfo);

  // Feed the encoded bytes via a custom in-memory source manager instead of
  // jpeg_mem_src(), which is unavailable in legacy IJG libjpeg v6b/v7.
  skity_jpeg_source jpeg_src((const unsigned char*)data_->RawData(),
                             data_->Size());
  cinfo.src = &jpeg_src;

  if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
    jpeg_destroy_decompress(&cinfo);
    return nullptr;
  }

  cinfo.out_color_space = JCS_EXT_RGBA;

  if (!jpeg_start_decompress(&cinfo)) {
    jpeg_destroy_decompress(&cinfo);
    return nullptr;
  }

  width = cinfo.output_width;
  height = cinfo.output_height;

  if (width == 0 || height == 0) {
    jpeg_destroy_decompress(&cinfo);
    return nullptr;
  }

  size_t row_bytes = static_cast<size_t>(width) * tjPixelSize[TJPF_RGBA];
  size_t pixel_size = row_bytes * height;

  pixels = reinterpret_cast<uint8_t*>(tjAlloc(static_cast<int>(pixel_size)));
  if (!pixels) {
    jpeg_destroy_decompress(&cinfo);
    return nullptr;
  }
  uint8_t* buf = pixels;

  // Initialize to opaque black. JPEG has no alpha channel, so decoded pixels
  // are fully opaque; pre-filling keeps undecoded trailing scanlines consistent
  // and avoids leaking uninitialized memory into the partial result.
  memset(buf, 0, pixel_size);
  for (size_t i = 0; i < pixel_size; i += tjPixelSize[TJPF_RGBA]) {
    buf[i + 3] = 0xFF;
  }

  // Decode scanline by scanline. On corrupt data libjpeg resyncs within the
  // decoded region (producing mosaic artifacts); on truncation it inserts a
  // fake EOI via a warning and yields the buffered rows. A truly fatal error
  // mid-stream longjmps to the block above, returning the partial result.
  while (cinfo.output_scanline < height) {
    uint8_t* row = buf + static_cast<size_t>(cinfo.output_scanline) * row_bytes;
    jpeg_read_scanlines(&cinfo, &row, 1);
  }

  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  auto image_data = skity::Data::MakeWithCopy(buf, pixel_size);
  tjFree(buf);
  return std::make_shared<Pixmap>(image_data, row_bytes, width, height);
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
