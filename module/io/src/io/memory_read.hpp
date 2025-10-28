// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_MEMORY_READ_HPP
#define MODULE_IO_SRC_IO_MEMORY_READ_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/image_filter.hpp>
#include <skity/effect/mask_filter.hpp>
#include <skity/effect/path_effect.hpp>
#include <skity/effect/shader.hpp>
#include <skity/graphic/color.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/io/data.hpp>
#include <string>

namespace skity {

struct FactorySet;
struct TypefaceSet;

class ReadBuffer {
 public:
  ReadBuffer() = default;

  ReadBuffer(const void* data, size_t size) { SetMemory(data, size); }

  ~ReadBuffer() = default;

  void SetMemory(const void* data, size_t size);

  void SetFactorySet(FactorySet* factory_set);

  void SetTypefaceSet(TypefaceSet* typeface_set) {
    typeface_set_ = typeface_set;
  }

  std::string GetFactoryName(int32_t index) const;

  void SetVersion(int32_t version) { version_ = version; }
  int32_t GetVersion() const { return version_; }

  bool IsVersionLT(int32_t target_version) const {
    return version_ > 0 && version_ < target_version;
  }

  size_t GetSize() const { return stop_ - base_; }

  size_t GetOffset() const { return curr_ - base_; }

  bool IsEOF() const { return curr_ >= stop_; }

  bool IsValid() const { return !is_error_; }

  const void* Skip(size_t size);
  const void* Skip(size_t count, size_t size);

  const void* SkipByteArray(size_t& size);

  size_t Available() const { return stop_ - curr_; }

  bool IsAvailable(size_t size) const { return Available() >= size; }

  uint32_t GetArrayCount();

  bool ReadPad32(void* buffer, size_t bytes);

  size_t ReadString(std::string& str);

  bool SkipToAlign4();

  template <typename T>
  bool ValidateCanReadN(size_t n) {
    return this->Validate(n <= (this->Available() / sizeof(T)));
  }

  uint8_t PeekByte();

  bool ReadBool();
  int32_t ReadInt();
  uint32_t ReadU32();

  float ReadFloat();

  Color ReadColor();

  Color4f ReadColor4f();

  Vec2 ReadPoint();

  SamplingOptions ReadSamplingOptions();

  bool Validate(bool is_valid);

  std::optional<Paint> ReadPaint();
  std::optional<Matrix> ReadMatrix();
  std::optional<Path> ReadPath();
  std::optional<RRect> ReadRRect();
  std::optional<Rect> ReadRect();

  std::optional<Path> ReadRegionAsPath();

  std::shared_ptr<Image> ReadImage();

  std::shared_ptr<Shader> ReadShader();
  std::shared_ptr<MaskFilter> ReadMaskFilter();
  std::shared_ptr<PathEffect> ReadPathEffect();
  std::shared_ptr<ImageFilter> ReadImageFilter();
  std::shared_ptr<ColorFilter> ReadColorFilter();

  std::shared_ptr<Typeface> ReadTypeface();

  template <typename T>
  std::shared_ptr<T> ReadFlattenable() {
    auto flattenable = this->ReadRawFlattenable();

    return std::static_pointer_cast<T>(flattenable);
  }

  bool ReadArray(void* buffer, size_t size, size_t element_size);

  template <typename T>
  bool ReadArrayN(void* buffer, size_t n) {
    return this->ReadArray(buffer, n, sizeof(T));
  }

  std::shared_ptr<Data> ReadByteArrayAsData();

  std::shared_ptr<Flattenable> ReadRawFlattenable();

 private:
  void SetInvalid();

 private:
  const char* curr_ = nullptr;
  const char* stop_ = nullptr;
  const char* base_ = nullptr;

  int32_t version_ = 0;

  FactorySet* factory_set_ = nullptr;
  size_t factory_count = 0;

  TypefaceSet* typeface_set_ = nullptr;

  bool is_error_ = false;
};

template <typename T>
T ReadFromMemory(ReadBuffer& buffer);

template <typename T>
void SkipFromMemory(ReadBuffer& buffer);

using FactoryProc = std::shared_ptr<Flattenable> (*)(const std::string&,
                                                     ReadBuffer&);

}  // namespace skity

#endif  // MODULE_IO_SRC_IO_MEMORY_READ_HPP
