// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_IO_PIXMAP_HPP
#define INCLUDE_SKITY_IO_PIXMAP_HPP

#include <atomic>
#include <memory>
#include <skity/graphic/alpha_type.hpp>
#include <skity/graphic/color_type.hpp>
#include <skity/macros.hpp>
#include <vector>

namespace skity {

class Data;

/**
 * Simple utility class to manage raw pixel data
 *
 */
class SKITY_API Pixmap final {
 public:
  struct PixelsChangeListener {
    virtual ~PixelsChangeListener() = default;
    virtual void OnPixelsChange(uint32_t id) = 0;
  };

  Pixmap()
      : data_(),
        pixels_(nullptr),
        id_(0),
        row_bytes_(0),
        width_(0),
        height_(0),
        alpha_type_(AlphaType::kUnknown_AlphaType),
        color_type_(ColorType::kUnknown) {}
  Pixmap(std::shared_ptr<Data> data, size_t rowBytes, uint32_t width,
         uint32_t height, AlphaType alpha_type = AlphaType::kUnpremul_AlphaType,
         ColorType color_type = ColorType::kRGBA);

  Pixmap(std::shared_ptr<Data> data, uint32_t width, uint32_t height,
         AlphaType alpha_type = AlphaType::kUnpremul_AlphaType,
         ColorType color_type = ColorType::kRGBA);

  Pixmap(uint32_t width, uint32_t height,
         AlphaType alpha_type = AlphaType::kUnpremul_AlphaType,
         ColorType color_type = ColorType::kRGBA);
  ~Pixmap();

  /**
   * Sets width, height, row bytes to zero; pixel address to nullptr
   */
  void Reset();

  size_t RowBytes() const { return row_bytes_; }
  const void* Addr() const { return pixels_; }
  void* WritableAddr() { return pixels_; }

  const uint8_t* Addr8(uint32_t x, uint32_t y) const;
  uint8_t* WritableAddr8(uint32_t x, uint32_t y);
  const uint16_t* Addr16(uint32_t x, uint32_t y) const;
  uint16_t* WritableAddr16(uint32_t x, uint32_t y);

  uint32_t Width() const { return width_; }
  uint32_t Height() const { return height_; }

  /*
   * Deprecated, use SetColorInfo instead
   */
  bool SetAlphaType(AlphaType alpha_type);
  AlphaType GetAlphaType() const { return alpha_type_; }

  /*
   * Deprecated, use SetColorInfo instead
   */
  void SetColorType(ColorType type);
  ColorType GetColorType() const { return color_type_; }

  bool SetColorInfo(AlphaType alpha_type, ColorType color_type);

  /** Returns a non-zero, unique value corresponding to the pixels in this
      pixmap. Each time the pixels are changed (and NotifyPixelsChanged is
      called), a different generation ID will be returned.
  */
  uint32_t GetID() const;

  void AddPixelsChangeListener(std::weak_ptr<PixelsChangeListener>);

  /**
   *  Call this if you have changed the contents of the pixels. This will in-
   *  turn cause a different generation ID value to be returned from
   *  GetID().
   */
  void NotifyPixelsChanged();

 private:
  Pixmap(const Pixmap&) = delete;
  Pixmap(Pixmap&&) = delete;
  Pixmap& operator=(const Pixmap&) = delete;
  Pixmap& operator=(Pixmap&&) = delete;

  // hold this to make sure data not release
  std::shared_ptr<Data> data_;
  void* pixels_;
  mutable std::atomic<uint32_t> id_;
  size_t row_bytes_;
  uint32_t width_;
  uint32_t height_;
  AlphaType alpha_type_;
  ColorType color_type_;

  std::vector<std::weak_ptr<PixelsChangeListener>> pixels_change_listeners_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_IO_PIXMAP_HPP
