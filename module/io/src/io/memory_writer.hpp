// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_MEMORY_WRITER_HPP
#define MODULE_IO_SRC_IO_MEMORY_WRITER_HPP

#include <skity/io/flattenable.hpp>
#include <skity/io/stream.hpp>
#include <skity/skity.hpp>
#include <string_view>
#include <vector>

namespace skity {

class MemoryWriter32;
struct SerialProc;

template <typename T>
static constexpr T Align4(T x) {
  return (x + 3) >> 2 << 2;
}

template <typename T>
void FlatIntoMemory(const T& value, MemoryWriter32& writer);

template <typename T>
void FlatIntoMemory(const T& value, WriteBuffer& buffer);

/**
 * Always write 32 bits data
 */
class MemoryWriter32 final {
 public:
  MemoryWriter32() = default;
  ~MemoryWriter32() = default;

  size_t BytesWritten() const { return memory_data_.size(); }

  // size should be multiple of 4
  uint32_t* Reserve(size_t size);
  uint32_t* ReservePad(size_t size);

  void Write(const void* data, size_t size);
  void WritePad(const void* data, size_t size);

  void WriteBool(bool b);
  void WriteInt32(int32_t i);
  void WriteUint32(uint32_t i);
  void Write8(uint8_t i);
  void Write16(uint16_t i);
  void WriteFloat(float f);

  void WriteString(const char* str, size_t len);

  void WriteVec2(const Vec2& v);
  void WriteRect(const Rect& rect);
  void WriteRRect(const RRect& rrect);
  void WritePath(const Path& path);

  void WriteMatrix(const Matrix& m);

  void WriteSampling(const SamplingOptions& sampling);

  const uint8_t* GetData() const { return memory_data_.data(); }

  template <typename T>
  const T& ReadAt(size_t offset) const {
    return *reinterpret_cast<const T*>(memory_data_.data() + offset);
  }

  template <typename T>
  void OverwriteAt(size_t offset, const T& value) {
    *reinterpret_cast<T*>(memory_data_.data() + offset) = value;
  }

  void WriteToStream(WriteStream& stream);

  std::shared_ptr<Data> MakeSnapshot();

 private:
  void GrowToAtLeast(size_t size);

 private:
  std::vector<uint8_t> memory_data_;
};

/**
 * SegmentBufferWriter is a writer that writes data to a segment of
 * memory.
 *
 */
class SegmentBufferWriter {
 public:
  SegmentBufferWriter();
  explicit SegmentBufferWriter(uint8_t* data);
  SegmentBufferWriter(uint8_t* data, size_t size);

  ~SegmentBufferWriter() = default;

  void Reset(uint8_t* data);
  void Reset(uint8_t* data, size_t size);

  size_t Pos() const { return pos_ - data_; }

  uint8_t* Skip(size_t size);

  size_t PadToAlign4();

  void Write(const void* buffer, size_t size);
  void WritePtr(const void* ptr);
  void WriteFloat(float f);
  void Write32(uint32_t i);
  void Write16(uint16_t i);
  void Write8(uint8_t i);
  void WriteBool(bool b);

 private:
  void WriteNoSizeCheck(const void* buffer, size_t size);

 private:
  uint8_t* data_;
  uint8_t* pos_;
  uint8_t* end_;
};

struct TypefaceSet;
struct FactorySet;

enum WriteBufferImageFlags {
  kVersion_bits = 8,
  kCurrVersion = 0,

  kHasSubsetRect = 1 << 8,
  kHasMipmap = 1 << 9,
  kUnpremul = 1 << 10,
};

/**
 * Convert draw object into flat buffer
 */
class BinaryWriteBuffer : public WriteBuffer {
 public:
  BinaryWriteBuffer() = default;
  ~BinaryWriteBuffer() override = default;

  void SetTypefaceSet(TypefaceSet* typeface_set) {
    typeface_set_ = typeface_set;
  }

  void SetFactorySet(FactorySet* factory_set) { factory_set_ = factory_set; }

  void SetSerialProc(const SerialProc* proc) { serial_proc_ = proc; }

  size_t BytesWritten() const { return writer_.BytesWritten(); }

  void WriteByteArray(const uint8_t* data, size_t size) override;
  void WriteBool(bool b) override;

  void WriteFloat(float f) override;
  void WriteFloatArray(const float* data, size_t size) override;

  void WriteInt32(int32_t i) override;
  void WriteUint32(uint32_t i) override;
  void WriteIntArray(const int32_t* data, size_t size);

  void WriteString(std::string_view str);

  void WriteColor(Color c) override;
  void WriteColorArray(const Color* data, size_t size) override;
  void WriteColor4f(const Color4f& c) override;
  void WriteColor4fArray(const Color4f* data, size_t size) override;

  void WritePoint(const Vec2& point) override;
  void WritePointArray(const Vec2* data, size_t size) override;

  void WriteMatrix(const Matrix& m) override;

  void WriteRect(const Rect& rect) override;

  void WriteRRect(const RRect& rrect);

  void WriteSampling(const SamplingOptions& sampling) override;

  void WritePath(const Path& path);

  void WriteImage(const Image* image) override;

  void WriteTypeface(const std::shared_ptr<Typeface>& typeface) override;

  void WritePaint(const Paint& paint);

  void WriteTextBlob(const TextBlob& blob);

  void WriteFlattenable(const Flattenable* flattenable) override;

  void WriteToStream(WriteStream& stream) { writer_.WriteToStream(stream); }

 private:
  TypefaceSet* typeface_set_ = nullptr;
  FactorySet* factory_set_ = nullptr;
  const SerialProc* serial_proc_ = nullptr;

  MemoryWriter32 writer_;
};

}  // namespace skity

#endif  // MODULE_IO_SRC_IO_MEMORY_WRITER_HPP
