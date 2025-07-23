// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_PIXMAP_SHADER_HPP
#define SRC_EFFECT_PIXMAP_SHADER_HPP

#include <skity/effect/shader.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/tile_mode.hpp>

namespace skity {

class PixmapShader : public Shader {
 public:
  explicit PixmapShader(std::shared_ptr<Image> image,
                        const SamplingOptions& sampling, TileMode x_tile_mode,
                        TileMode y_tile_mode, const Matrix& local_matrix);
  ~PixmapShader() override = default;

  const std::shared_ptr<Image>* AsImage() const override;

  const SamplingOptions* GetSamplingOptions() const override;

  TileMode GetXTileMode() const;

  TileMode GetYTileMode() const;

 private:
  std::shared_ptr<Image> image_;
  SamplingOptions sampling_;
  TileMode x_tile_mode_;
  TileMode y_tile_mode_;
};

}  // namespace skity

#endif  // SRC_EFFECT_PIXMAP_SHADER_HPP
