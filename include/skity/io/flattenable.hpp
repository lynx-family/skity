// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_IO_FLATTENABLE_HPP
#define INCLUDE_SKITY_IO_FLATTENABLE_HPP

#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/macros.hpp>
#include <string_view>

namespace skity {

class Flattenable;
class Image;
class Typeface;

/**
 * Abstract class to write some common geometry data into a binary buffer.
 * The implementation is located in serialize module.
 */
class SKITY_API WriteBuffer {
 public:
  virtual ~WriteBuffer() = default;

  /**
   * @brief Write a byte array into the buffer.
   *
   * @param data  The byte array to write.
   * @param size  The size of byte array.
   */
  virtual void WriteByteArray(const uint8_t* data, size_t size) = 0;

  virtual void WriteBool(bool b) = 0;
  virtual void WriteFloat(float f) = 0;
  virtual void WriteFloatArray(float const* array, size_t count) = 0;

  virtual void WriteInt32(int32_t i) = 0;
  virtual void WriteUint32(uint32_t i) = 0;

  virtual void WriteColor(Color c) = 0;
  virtual void WriteColorArray(Color const* array, size_t count) = 0;
  virtual void WriteColor4f(const Color4f& c) = 0;
  virtual void WriteColor4fArray(const Color4f* array, size_t count) = 0;

  virtual void WritePoint(const Vec2& point) = 0;
  virtual void WritePointArray(const Vec2* data, size_t size) = 0;

  virtual void WriteSampling(const SamplingOptions& sampling) = 0;

  virtual void WriteMatrix(const Matrix& matrix) = 0;

  virtual void WriteRect(const Rect& rect) = 0;

  virtual void WriteImage(const Image* image) = 0;

  virtual void WriteTypeface(const std::shared_ptr<Typeface>& typeface) = 0;

  virtual void WriteFlattenable(const Flattenable* flattenable) = 0;
};

/**
 * @brief Flattenable object can be flattened into a binary buffer.
 *
 * @note We only use this interface if the object does not have public interface
 * to query all necessary data to flatten.
 *
 */
class SKITY_API Flattenable {
 public:
  virtual ~Flattenable() = default;

  /**
   * @brief Get the Proc Name object,
   *        ProcName is used to identify the object type.
   * @note  The ProcName is only used in skp file.
   *
   * @return std::string_view
   */
  virtual std::string_view ProcName() const = 0;

  /**
   * @brief Flatten object into a binary buffer.
   *
   * @param buffer  The buffer to store flattened data. Can be nullptr.
   *
   * @return size_t  The size of flattened data used.
   */
  virtual void FlattenToBuffer(WriteBuffer& buffer) const = 0;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_IO_FLATTENABLE_HPP
