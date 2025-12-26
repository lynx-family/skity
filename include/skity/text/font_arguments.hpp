// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_FONT_ARGUMENTS_HPP
#define INCLUDE_SKITY_TEXT_FONT_ARGUMENTS_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <skity/macros.hpp>
#include <vector>

namespace skity {

typedef uint32_t FourByteTag;
static inline constexpr FourByteTag SetFourByteTag(char a, char b, char c,
                                                   char d) {
  return (((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8) |
          (uint32_t)d);
}

class VariationAxis {
 public:
  constexpr VariationAxis() : tag(0), min(0), def(0), max(0), hidden(false) {}
  constexpr VariationAxis(FourByteTag tag, float min, float def, float max,
                          bool hidden)
      : tag(tag), min(min), def(def), max(max), hidden(hidden) {}

  // Four character identifier of the font axis (weight, width, slant,
  // italic...).
  FourByteTag tag;
  // Minimum value supported by this axis.
  float min;
  // Default value set by this axis.
  float def;
  // Maximum value supported by this axis. The maximum can equal the minimum.
  float max;
  // Return whether this axis is recommended to be remain hidden in user
  // interfaces.
  bool hidden;
};

class VariationPosition {
 public:
  struct Coordinate {
    Coordinate(const FourByteTag axis, const float value)
        : axis(axis), value(value) {}
    FourByteTag axis;
    float value;
  };

  VariationPosition() = default;

  void AddCoordinate(const FourByteTag axis, const float value) {
    coordinates.emplace_back(axis, value);
  }

  const std::vector<Coordinate>& GetCoordinates() const { return coordinates; }

  bool operator==(const VariationPosition& other) const {
    if (coordinates.size() != other.coordinates.size()) {
      return false;
    }

    auto a = coordinates;
    auto b = other.coordinates;

    auto cmp = [](const Coordinate& x, const Coordinate& y) {
      if (x.axis != y.axis) return x.axis < y.axis;
      return x.value < y.value;
    };

    std::sort(a.begin(), a.end(), cmp);
    std::sort(b.begin(), b.end(), cmp);

    constexpr float kEpsilon = 1e-6f;
    for (size_t i = 0; i < a.size(); ++i) {
      if (a[i].axis != b[i].axis) return false;
      if (std::fabs(a[i].value - b[i].value) > kEpsilon) return false;
    }

    return true;
  }

  bool operator!=(const VariationPosition& other) const {
    return !(*this == other);
  }

 private:
  std::vector<Coordinate> coordinates;
};

class FontArguments {
 public:
  FontArguments() : collection_index_(0), variation_pos_() {}

  FontArguments& SetCollectionIndex(int collection_index) {
    collection_index_ = collection_index;
    return *this;
  }

  size_t GetCollectionIndex() const { return collection_index_; }

  FontArguments& SetVariationDesignPosition(const VariationPosition& position) {
    variation_pos_ = position;
    return *this;
  }

  const VariationPosition& GetVariationDesignPosition() const {
    return variation_pos_;
  }

 private:
  size_t collection_index_;
  VariationPosition variation_pos_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_FONT_ARGUMENTS_HPP
