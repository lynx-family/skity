// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_SW_SW_A8_DRAWABLE_HPP
#define SRC_RENDER_SW_SW_A8_DRAWABLE_HPP

#include <cassert>
#include <cstring>
#include <skity/graphic/color_type.hpp>
#include <skity/graphic/path.hpp>
#include <skity/io/pixmap.hpp>

#include "src/render/sw/sw_raster.hpp"

namespace skity {

struct SWA8Drawable : public SpanBuilderDelegate {
 public:
  explicit SWA8Drawable(std::shared_ptr<Pixmap> pixmap)
      : pixmap_(pixmap),
        pixel_addr_(static_cast<uint8_t*>(const_cast<void*>(pixmap_->Addr()))),
        row_bytes_(pixmap_->RowBytes()) {
    assert(pixmap_->GetColorType() == ColorType::kA8);
  }

  void OnBuildSpan(int x, int y, int width, const uint8_t alpha) override;

  void Draw(Path const& path, Matrix const& transform);

 private:
  std::shared_ptr<Pixmap> pixmap_;
  uint8_t* pixel_addr_;
  size_t row_bytes_;
};

}  // namespace skity

#endif  // SRC_RENDER_SW_SW_A8_DRAWABLE_HPP
