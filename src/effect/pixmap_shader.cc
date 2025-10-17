// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/effect/pixmap_shader.hpp"

#include <skity/io/pixmap.hpp>

namespace skity {

PixmapShader::PixmapShader(std::shared_ptr<Image> image,
                           const SamplingOptions& sampling,
                           TileMode x_tile_mode, TileMode y_tile_mode,
                           const Matrix& local_matrix)
    : Shader(),
      image_(std::move(image)),
      sampling_(sampling),
      x_tile_mode_(x_tile_mode),
      y_tile_mode_(y_tile_mode) {
  SetLocalMatrix(local_matrix);
}

const std::shared_ptr<Image>* PixmapShader::AsImage() const { return &image_; }

const SamplingOptions* PixmapShader::GetSamplingOptions() const {
  return &sampling_;
}

TileMode PixmapShader::GetXTileMode() const { return x_tile_mode_; }

TileMode PixmapShader::GetYTileMode() const { return y_tile_mode_; }

std::string_view PixmapShader::ProcName() const { return "SkImageShader"; }

void PixmapShader::FlattenToBuffer(WriteBuffer& buffer) const {
  buffer.WriteUint32(static_cast<uint32_t>(x_tile_mode_));
  buffer.WriteUint32(static_cast<uint32_t>(y_tile_mode_));

  buffer.WriteSampling(sampling_);

  buffer.WriteImage(image_.get());

  buffer.WriteBool(true);  // is raw
}

}  // namespace skity
