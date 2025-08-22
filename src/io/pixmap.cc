// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <algorithm>
#include <cstring>
#include <skity/io/data.hpp>
#include <skity/io/pixmap.hpp>
#include <utility>

#include "src/graphic/color_priv.hpp"

namespace skity {

static size_t BytesPerPixel(ColorType color_type) {
  switch (color_type) {
    case ColorType::kRGBA:
    case ColorType::kBGRA:
      return 4;
    case ColorType::kRGB565:
      return 2;
    case ColorType::kA8:
      return 1;
    case ColorType::kUnknown:
      return 0;
  }
}

static bool IsAlphaMatchColorType(AlphaType alpha_type, ColorType color_type) {
  switch (color_type) {
    case ColorType::kUnknown:
      return alpha_type == kUnknown_AlphaType;
    case ColorType::kRGBA:
    case ColorType::kBGRA:
    case ColorType::kA8:
      return alpha_type == kPremul_AlphaType ||
             alpha_type == kUnpremul_AlphaType;
    case ColorType::kRGB565:
      return alpha_type == kOpaque_AlphaType;
  }
}

Pixmap::Pixmap(std::shared_ptr<Data> data, uint32_t width, uint32_t height,
               AlphaType alpha_type, ColorType color_type)
    : Pixmap(data, width * BytesPerPixel(color_type), width, height, alpha_type,
             color_type) {}

Pixmap::Pixmap(std::shared_ptr<Data> data, size_t rowBytes, uint32_t width,
               uint32_t height, AlphaType alpha_type, ColorType color_type)
    : data_(std::move(data)),
      pixels_(nullptr),
      id_(0),
      row_bytes_(rowBytes),
      width_(width),
      height_(height),
      alpha_type_(alpha_type),
      color_type_(color_type) {
  if (data_) {
    pixels_ = const_cast<void*>(data_->RawData());
  }
}

Pixmap::Pixmap(uint32_t width, uint32_t height, AlphaType alpha_type,
               ColorType color_type)
    : pixels_(nullptr),
      id_(0),
      row_bytes_(width * BytesPerPixel(color_type)),
      width_(width),
      height_(height),
      alpha_type_(alpha_type),
      color_type_(color_type) {
  if (width <= 0 || height <= 0) {
    return;
  }
  size_t len = row_bytes_ * height;
  data_ = skity::Data::MakeFromMalloc(std::calloc(len, 1), len);
  pixels_ = const_cast<void*>(data_->RawData());
}

Pixmap::~Pixmap() { NotifyPixelsChanged(); }

void Pixmap::Reset() {
  data_ = nullptr;
  pixels_ = nullptr;
  row_bytes_ = 0;
  width_ = 0;
  height_ = 0;
  NotifyPixelsChanged();
}

bool Pixmap::SetAlphaType(AlphaType alpha_type) { return false; }

bool Pixmap::SetColorInfo(AlphaType alpha_type, ColorType color_type) {
  if (!IsAlphaMatchColorType(alpha_type, color_type)) {
    return false;
  }

  if ((alpha_type_ == kPremul_AlphaType && alpha_type == kUnpremul_AlphaType) ||
      (alpha_type_ == kUnpremul_AlphaType && alpha_type == kPremul_AlphaType)) {
    auto f = alpha_type == kPremul_AlphaType ? ColorToPMColor : PMColorToColor;
    uint8_t* pixels = reinterpret_cast<uint8_t*>(const_cast<void*>(pixels_));
    for (size_t r = 0; r < height_; ++r) {
      uint32_t* p = reinterpret_cast<uint32_t*>(pixels + row_bytes_ * r);
      for (size_t c = 0; c < width_; ++c) {
        *p = f(*p);
        ++p;
      }
    }
  }

  alpha_type_ = alpha_type;
  color_type_ = color_type;

  return true;
}

void Pixmap::SetColorType(ColorType type) {}

static uint32_t NextID() {
  static std::atomic<uint32_t> nextID{2};
  uint32_t id;
  do {
    id = nextID.fetch_add(2, std::memory_order_relaxed);
  } while (id == 0);
  return id;
}

uint32_t Pixmap::GetID() const {
  uint32_t id = id_.load();
  if (0 == id) {
    uint32_t next = NextID();
    if (id_.compare_exchange_strong(id, next)) {
      id = next;  // There was no race or we won the race.  id_ is next now.
    } else {
      // We lost a race to set id_. compare_exchange() filled id with the
      // winner.
    }
  }
  return id;
}

void Pixmap::AddPixelsChangeListener(
    std::weak_ptr<PixelsChangeListener> listener) {
  auto sp = listener.lock();
  if (!sp) {
    return;
  }
  for (auto it = pixels_change_listeners_.begin();
       it != pixels_change_listeners_.end(); ++it) {
    if (it->lock() == sp) {
      // re-add listeners if re-create textures
      return;
    }
  }
  pixels_change_listeners_.emplace_back(std::move(listener));
}

void Pixmap::NotifyPixelsChanged() {
  for (auto& listener : pixels_change_listeners_) {
    if (auto sp = listener.lock()) {
      sp->OnPixelsChange(id_);
    }
  }
  // listeners are invalid
  pixels_change_listeners_.clear();
  id_.store(0);
}

const uint8_t* Pixmap::Addr8(uint32_t x, uint32_t y) const {
  if (x >= width_ || y >= height_) {
    return nullptr;
  }
  return reinterpret_cast<uint8_t*>(pixels_) + y * row_bytes_ +
         x * BytesPerPixel(color_type_);
}

uint8_t* Pixmap::WritableAddr8(uint32_t x, uint32_t y) {
  return const_cast<uint8_t*>(Addr8(x, y));
}

const uint16_t* Pixmap::Addr16(uint32_t x, uint32_t y) const {
  if (x >= width_ || y >= height_) {
    return nullptr;
  }
  return reinterpret_cast<const uint16_t*>(Addr8(x, y));
}

uint16_t* Pixmap::WritableAddr16(uint32_t x, uint32_t y) {
  return const_cast<uint16_t*>(Addr16(x, y));
}

}  // namespace skity
