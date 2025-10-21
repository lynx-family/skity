// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/memory_read.hpp"

#include <skity/codec/codec.hpp>
#include <skity/io/picture.hpp>

#include "src/io/memory_writer.hpp"
#include "src/picture_priv.hpp"

namespace skity {

FactoryProc GetShaderFactoryProc(const std::string& factory_name);
FactoryProc GetPathEffectFactoryProc(const std::string& factory_name);
FactoryProc GetMaskFilterFactoryProc(const std::string& factory_name);
FactoryProc GetColorFilterFactoryProc(const std::string& factory_name);
FactoryProc GetImageFilterFactoryProc(const std::string& factory_name);

namespace {
FactoryProc factory_to_proc(const std::string& factory_name) {
  FactoryProc proc = nullptr;

  if ((proc = GetPathEffectFactoryProc(factory_name)) != nullptr) {
    return proc;
  }

  if ((proc = GetShaderFactoryProc(factory_name)) != nullptr) {
    return proc;
  }

  if ((proc = GetMaskFilterFactoryProc(factory_name)) != nullptr) {
    return proc;
  }

  if ((proc = GetColorFilterFactoryProc(factory_name)) != nullptr) {
    return proc;
  }

  if ((proc = GetImageFilterFactoryProc(factory_name)) != nullptr) {
    return proc;
  }

  return nullptr;
}

}  // namespace

void ReadBuffer::SetMemory(const void* data, size_t size) {
  curr_ = base_ = static_cast<const char*>(data);
  stop_ = curr_ + size;
}

void ReadBuffer::SetFactorySet(FactorySet* factory_set) {
  factory_set_ = factory_set;

  if (factory_set_) {
    factory_count = factory_set_->GetFactoryCount();
  }
}

std::string ReadBuffer::GetFactoryName(int32_t index) const {
  if (factory_set_) {
    return factory_set_->GetFactoryName(index);
  }

  return {};
}

const void* ReadBuffer::Skip(size_t size) {
  size_t inc = Align4(size);

  Validate(inc >= size);

  const void* addr = curr_;

  Validate(IsAvailable(inc));

  if (is_error_) {
    return nullptr;
  }

  curr_ += inc;

  return addr;
}

const void* ReadBuffer::Skip(size_t count, size_t size) {
  size_t inc = count * size;

  return Skip(inc);
}

const void* ReadBuffer::SkipByteArray(size_t& size) {
  auto count = ReadU32();

  auto buf = Skip(count);

  if (IsValid()) {
    size = count;
  } else {
    size = 0;
  }

  return buf;
}

bool ReadBuffer::Validate(bool is_valid) {
  if (!is_valid) {
    SetInvalid();
  }

  return !is_error_;
}

uint32_t ReadBuffer::GetArrayCount() {
  constexpr size_t inc = sizeof(uint32_t);

  if (!Validate(IsAvailable(inc))) {
    return 0;
  }

  return *(reinterpret_cast<const uint32_t*>(curr_));
}

bool ReadBuffer::ReadPad32(void* buffer, size_t bytes) {
  auto src = Skip(bytes);

  if (!src) {
    return false;
  }

  memcpy(buffer, src, bytes);

  return true;
}

size_t ReadBuffer::ReadString(std::string& str) {
  auto len = ReadU32();

  if (len == 0) {
    return 0;
  }

  auto c_str = static_cast<const char*>(
      Skip(len + 1));  // skip the null-terminated string

  if (!Validate(c_str && c_str[len] == '\0')) {
    return 0;
  }

  str = std::string(c_str, len);

  return len + 1;
}

bool ReadBuffer::SkipToAlign4() {
  auto pos = reinterpret_cast<intptr_t>(curr_);
  size_t n = Align4(pos) - pos;

  if (IsValid() && n <= Available()) {
    curr_ += n;
  } else {
    SetInvalid();
  }

  return IsValid();
}

uint8_t ReadBuffer::PeekByte() {
  if (Available() <= 0) {
    is_error_ = true;
    return 0;
  }

  return *reinterpret_cast<const uint8_t*>(curr_);
}

bool ReadBuffer::ReadBool() {
  uint32_t value = ReadU32();

  Validate(!(value & ~0x1));

  return value != 0;
}

int32_t ReadBuffer::ReadInt() {
  constexpr size_t kIntSize = sizeof(int32_t);

  if (!Validate(IsAvailable(kIntSize))) {
    return 0;
  }

  int32_t value = *(reinterpret_cast<const int32_t*>(curr_));

  curr_ += kIntSize;

  return value;
}

uint32_t ReadBuffer::ReadU32() { return ReadInt(); }

float ReadBuffer::ReadFloat() {
  constexpr size_t kFloatSize = sizeof(float);

  if (!Validate(IsAvailable(kFloatSize))) {
    return 0.f;
  }

  float value = *(reinterpret_cast<const float*>(curr_));

  curr_ += kFloatSize;

  return value;
}

Color ReadBuffer::ReadColor() { return ReadU32(); }

Color4f ReadBuffer::ReadColor4f() {
  Color4f color{};

  if (!ReadPad32(&color, sizeof(color))) {
    return {};
  }

  return color;
}

std::shared_ptr<Data> ReadBuffer::ReadByteArrayAsData() {
  auto num_bytes = GetArrayCount();

  if (!Validate(IsAvailable(num_bytes))) {
    return {};
  }

  auto data = Data::MakeFromMalloc(std::malloc(num_bytes), num_bytes);

  if (!ReadArrayN<uint8_t>(const_cast<void*>(data->RawData()), num_bytes)) {
    return {};
  }

  return data;
}

std::shared_ptr<Flattenable> ReadBuffer::ReadRawFlattenable() {
  FactoryProc factory = nullptr;

  std::string factory_name;

  if (factory_count > 0) {
    auto factory_index = this->ReadInt();

    if (factory_index == 0 || !this->IsValid()) {
      return {};
    }

    uint32_t size_recorded = this->ReadU32();
    (void)size_recorded;

    factory_name = this->GetFactoryName(factory_index - 1);

    if (factory_name.empty()) {
      return {};
    }

    factory = factory_to_proc(factory_name);
  } else {
    if (PeekByte() != 0) {
      // If the first byte is no-zero, the factory name is specified by a string
      size_t ignored_length = ReadString(factory_name);

      if (!factory_name.empty()) {
        factory_set_->AddFactory(factory_name);
      }
    } else {
      // Read the index
      auto index = ReadU32() >> 8;

      if (index == 0) {
        return {};
      }

      factory_name = GetFactoryName(index);
    }

    if (factory_name.empty()) {
      return {};
    }

    factory = factory_to_proc(factory_name);

    uint32_t size_recorded = this->ReadU32();  // skip the size
    (void)size_recorded;
  }

  if (!Validate(factory != nullptr)) {
    return {};
  }

  return factory(factory_name, *this);
}

void ReadBuffer::SetInvalid() {
  if (!is_error_) {
    curr_ = stop_;
    is_error_ = true;
  }
}

std::optional<Paint> ReadBuffer::ReadPaint() {
  return ReadFromMemory<std::optional<Paint>>(*this);
}

std::optional<Matrix> ReadBuffer::ReadMatrix() {
  return ReadFromMemory<std::optional<Matrix>>(*this);
}

std::optional<Path> ReadBuffer::ReadPath() {
  return ReadFromMemory<std::optional<Path>>(*this);
}

std::optional<RRect> ReadBuffer::ReadRRect() {
  return ReadFromMemory<std::optional<RRect>>(*this);
}

std::optional<Rect> ReadBuffer::ReadRect() {
  Rect rect;

  if (!this->ReadPad32(&rect, sizeof(Rect))) {
    return {};
  }

  return {rect};
}

std::optional<Path> ReadBuffer::ReadRegionAsPath() { return std::nullopt; }

Vec2 ReadBuffer::ReadPoint() {
  Vec2 point;

  point.x = ReadFloat();
  point.y = ReadFloat();

  return point;
}

SamplingOptions ReadBuffer::ReadSamplingOptions() {
  if (!IsVersionLT(Version::kAnisotropicFilter)) {
    auto maxAniso = ReadInt();

    if (maxAniso != 0) {
      // we do not support anisotropic filter, just return the default one
      return SamplingOptions{};
    }
  }

  if (ReadBool()) {
    (void)ReadFloat();  // b
    (void)ReadFloat();  // c

    // we do not support cubic filter, just return the default one
    return SamplingOptions{};
  } else {
    auto filter = static_cast<FilterMode>(ReadU32());
    auto mipmap = static_cast<MipmapMode>(ReadU32());

    return SamplingOptions{filter, mipmap};
  }
}

std::shared_ptr<Image> ReadBuffer::ReadImage() {
  auto flags = ReadU32();

  std::optional<AlphaType> alpha_type = std::nullopt;

  if (flags & WriteBufferImageFlags::kUnpremul) {
    alpha_type = AlphaType::kUnpremul_AlphaType;
  }

  auto data = ReadByteArrayAsData();

  Validate(data != nullptr);

  auto codec = Codec::MakeFromData(data);

  std::shared_ptr<Image> image;

  if (codec) {
    codec->SetData(data);

    auto pixmap = codec->Decode();

    if (pixmap) {
      image = Image::MakeImage(std::move(pixmap));
    }
  }

  // keep going and skip rest of image data
  if (flags & WriteBufferImageFlags::kHasSubsetRect) {
    (void)ReadRect();
  }

  if (flags & WriteBufferImageFlags::kHasMipmap) {
    (void)ReadByteArrayAsData();
  }

  return image;
}

std::shared_ptr<Shader> ReadBuffer::ReadShader() {
  return ReadFlattenable<Shader>();
}

std::shared_ptr<MaskFilter> ReadBuffer::ReadMaskFilter() {
  return ReadFlattenable<MaskFilter>();
}

std::shared_ptr<PathEffect> ReadBuffer::ReadPathEffect() {
  return ReadFlattenable<PathEffect>();
}

std::shared_ptr<ImageFilter> ReadBuffer::ReadImageFilter() {
  return ReadFlattenable<ImageFilter>();
}

std::shared_ptr<ColorFilter> ReadBuffer::ReadColorFilter() {
  return ReadFlattenable<ColorFilter>();
}

std::shared_ptr<Typeface> ReadBuffer::ReadTypeface() {
  auto index = ReadInt();

  if (index == 0) {  // empty
    return {};
  } else if (index > 0) {
    if (!Validate(index <= typeface_set_->typefaces.size())) {
      return {};
    }

    return typeface_set_->typefaces[index - 1];
  } else {  // custom
    // we do not support custom typeface
    // so just try to use our font engine parse the following data

    auto size = -static_cast<size_t>(index);
    auto data = Skip(size);

    if (!Validate(data != nullptr)) {
      return {};
    }

    auto tf_data = Data::MakeWithCopy(data, size);

    auto fm = FontManager::RefDefault();

    auto typeface = fm->MakeFromData(tf_data);

    if (!Validate(typeface != nullptr)) {
      return {};
    }

    return typeface;
  }

  return {};
}

bool ReadBuffer::ReadArray(void* buffer, size_t size, size_t element_size) {
  auto count = ReadU32();

  return this->Validate(size == count) &&
         this->ReadPad32(buffer, size * element_size);
}

}  // namespace skity
