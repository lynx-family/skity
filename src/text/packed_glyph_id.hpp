// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PACKED_GLYPH_ID_HPP
#define SRC_TEXT_PACKED_GLYPH_ID_HPP

#include <cstddef>
#include <cstdint>
#include <functional>
#include <skity/text/glyph.hpp>

namespace skity {

// A glyph ID plus the two-bit X/Y subpixel phases used to rasterize its mask.
// The bit layout follows Skia's SkPackedGlyphID.
class PackedGlyphID final {
 public:
  static constexpr uint32_t kGlyphIDBits = 16u;
  static constexpr uint32_t kSubpixelPhaseBits = 2u;
  static constexpr uint32_t kSubpixelPhaseMask =
      (1u << kSubpixelPhaseBits) - 1u;

  constexpr explicit PackedGlyphID(GlyphID glyph_id)
      : value_(static_cast<uint32_t>(glyph_id) << kGlyphIDShift) {}

  constexpr PackedGlyphID(GlyphID glyph_id, uint8_t x_phase, uint8_t y_phase)
      : value_(Pack(glyph_id, x_phase, y_phase)) {}

  constexpr GlyphID GetGlyphID() const {
    return static_cast<GlyphID>((value_ >> kGlyphIDShift) & kGlyphIDMask);
  }

  constexpr uint8_t GetSubpixelXPhase() const {
    return static_cast<uint8_t>((value_ >> kSubpixelXShift) &
                                kSubpixelPhaseMask);
  }

  constexpr uint8_t GetSubpixelYPhase() const {
    return static_cast<uint8_t>((value_ >> kSubpixelYShift) &
                                kSubpixelPhaseMask);
  }

  constexpr uint32_t Value() const { return value_; }

  friend constexpr bool operator==(PackedGlyphID lhs, PackedGlyphID rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend constexpr bool operator!=(PackedGlyphID lhs, PackedGlyphID rhs) {
    return !(lhs == rhs);
  }

  struct Hash {
    size_t operator()(PackedGlyphID id) const {
      return std::hash<uint32_t>{}(id.Value());
    }
  };

 private:
  static constexpr uint32_t kSubpixelXShift = 0u;
  static constexpr uint32_t kGlyphIDShift = kSubpixelPhaseBits;
  static constexpr uint32_t kSubpixelYShift = kGlyphIDBits + kSubpixelPhaseBits;
  static constexpr uint32_t kGlyphIDMask = (1u << kGlyphIDBits) - 1u;

  static constexpr uint32_t Pack(GlyphID glyph_id, uint8_t x_phase,
                                 uint8_t y_phase) {
    return ((static_cast<uint32_t>(x_phase) & kSubpixelPhaseMask)
            << kSubpixelXShift) |
           (static_cast<uint32_t>(glyph_id) << kGlyphIDShift) |
           ((static_cast<uint32_t>(y_phase) & kSubpixelPhaseMask)
            << kSubpixelYShift);
  }

  uint32_t value_;
};

static_assert(sizeof(PackedGlyphID) == sizeof(uint32_t));

}  // namespace skity

#endif  // SRC_TEXT_PACKED_GLYPH_ID_HPP
