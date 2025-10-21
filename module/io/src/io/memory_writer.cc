// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/memory_writer.hpp"

#include <cstring>
#include <skity/codec/codec.hpp>
#include <skity/io/picture.hpp>

namespace skity {

namespace {

std::shared_ptr<Data> image_to_data(const Image* image,
                                    const SerialProc* serial_proc) {
  if (image == nullptr) {
    return {};
  }

  std::shared_ptr<Pixmap> pixmap;

  if (serial_proc && serial_proc->image_proc) {
    pixmap = serial_proc->image_proc(image);
  } else {
    // check if we can get pixmap direct from this image
    auto pm = image->GetPixmap();

    if (pm) {
      pixmap = *pm;
    }
  }

  if (!pixmap) {
    // check if we can read pixmap from this image
    if (serial_proc && serial_proc->gpu_context) {
      pixmap = image->ReadPixels(serial_proc->gpu_context);
    }
  }

  if (!pixmap) {
    return {};
  }

  auto cocec = Codec::MakePngCodec();

  if (!cocec) {
    return {};
  }

  return cocec->Encode(pixmap.get());
}

}  // namespace

void MemoryWriter32::WriteBool(bool b) { WriteUint32(b ? 1 : 0); }

void MemoryWriter32::WriteInt32(int32_t i) { WriteUint32(i); }

void MemoryWriter32::WriteUint32(uint32_t i) {
  auto data = Reserve(sizeof(i));

  *data = i;
}

void MemoryWriter32::Write8(uint8_t i) {
  auto data = Reserve(sizeof(i));

  *data = i;
}

void MemoryWriter32::Write16(uint16_t i) {
  auto data = Reserve(sizeof(i));

  *data = i;
}

void MemoryWriter32::WriteFloat(float f) {
  auto data = Reserve(sizeof(f));

  *reinterpret_cast<float*>(data) = f;
}

void MemoryWriter32::WriteString(const char* str, size_t len) {
  if (nullptr == str) {
    str = "";
    len = 0;
  }

  if (static_cast<int64_t>(len) < 0) {
    len = std::strlen(str);
  }

  // [ 4 byte len ] [ str ... ] [ 1 - 4 \0s ]
  auto ptr = ReservePad(sizeof(uint32_t) + len + 1);

  *ptr = static_cast<uint32_t>(len);

  auto chars = reinterpret_cast<char*>(ptr + 1);
  std::memcpy(chars, str, len);

  chars[len] = '\0';
}

void MemoryWriter32::WriteVec2(const Vec2& v) {
  auto data = Reserve(sizeof(v));
  *reinterpret_cast<Vec2*>(data) = v;
}

void MemoryWriter32::WriteRect(const Rect& rect) {
  auto data = Reserve(sizeof(rect));

  *reinterpret_cast<Rect*>(data) = rect;
}

void MemoryWriter32::WriteRRect(const RRect& rrect) {
  FlatIntoMemory(rrect, *this);
}

void MemoryWriter32::WritePath(const Path& path) {
  FlatIntoMemory(path, *this);
}

void MemoryWriter32::WriteMatrix(const Matrix& matrix) {
  FlatIntoMemory(matrix, *this);
}

void MemoryWriter32::WriteSampling(const SamplingOptions& sampling) {
  // dumy info to align with skia
  WriteUint32(0);  // maxAniso

  WriteBool(false);  // use cubic

  WriteUint32(static_cast<uint32_t>(sampling.filter));
  WriteUint32(static_cast<uint32_t>(sampling.mipmap));
}

uint32_t* MemoryWriter32::Reserve(size_t size) {
  size = Align4(size);

  auto offset = memory_data_.size();

  auto required = offset + size;

  if (required > memory_data_.capacity()) {
    GrowToAtLeast(required);
  }

  memory_data_.resize(required);

  return reinterpret_cast<uint32_t*>(memory_data_.data() + offset);
}

uint32_t* MemoryWriter32::ReservePad(size_t size) {
  size_t aligned_size = Align4(size);

  auto p = Reserve(aligned_size);

  if (aligned_size != size) {
    p[aligned_size / 4 - 1] = 0;
  }

  return p;
}

void MemoryWriter32::Write(const void* data, size_t size) {
  auto p = Reserve(size);

  std::memcpy(p, data, size);
}

void MemoryWriter32::WritePad(const void* data, size_t size) {
  auto p = ReservePad(size);

  if (size > 0) {
    std::memcpy(p, data, size);
  }
}

void MemoryWriter32::WriteToStream(WriteStream& stream) {
  stream.Write(memory_data_.data(), memory_data_.size());
}

std::shared_ptr<Data> MemoryWriter32::MakeSnapshot() {
  return Data::MakeWithCopy(memory_data_.data(), memory_data_.size());
}

void MemoryWriter32::GrowToAtLeast(size_t size) {
  static constexpr size_t kAtLeast = 4096;

  auto capacity = memory_data_.capacity();
  capacity = kAtLeast + std::max(size, capacity + capacity / 2);

  memory_data_.reserve(capacity);
}

SegmentBufferWriter::SegmentBufferWriter()
    : data_(nullptr), pos_(nullptr), end_(nullptr) {}

SegmentBufferWriter::SegmentBufferWriter(uint8_t* data)
    : data_(nullptr), pos_(nullptr), end_(nullptr) {
  Reset(data);
}

SegmentBufferWriter::SegmentBufferWriter(uint8_t* data, size_t size)
    : data_(nullptr), pos_(nullptr), end_(nullptr) {
  Reset(data, size);
}

void SegmentBufferWriter::Reset(uint8_t* data) {
  data_ = data;
  pos_ = data;

  end_ = nullptr;
}

void SegmentBufferWriter::Reset(uint8_t* data, size_t size) {
  data_ = data;
  pos_ = data;
  end_ = data + size;
}

uint8_t* SegmentBufferWriter::Skip(size_t size) {
  auto result = pos_;
  WriteNoSizeCheck(nullptr, size);

  return data_ == nullptr ? nullptr : result;
}

size_t SegmentBufferWriter::PadToAlign4() {
  auto pos = Pos();
  auto n = Align4(pos) - pos;

  if (n > 0 && data_ != nullptr) {
    auto p = pos_;
    auto stop = p + n;

    do {
      *p++ = 0;
    } while (p < stop);
  }

  pos_ += n;

  return n;
}

void SegmentBufferWriter::Write(const void* buffer, size_t size) {
  if (size > 0) {
    WriteNoSizeCheck(buffer, size);
  }
}

void SegmentBufferWriter::WritePtr(const void* ptr) {
  WriteNoSizeCheck(&ptr, sizeof(ptr));
}

void SegmentBufferWriter::WriteFloat(float f) {
  WriteNoSizeCheck(&f, sizeof(f));
}
void SegmentBufferWriter::Write32(uint32_t i) {
  WriteNoSizeCheck(&i, sizeof(i));
}
void SegmentBufferWriter::Write16(uint16_t i) {
  WriteNoSizeCheck(&i, sizeof(i));
}
void SegmentBufferWriter::Write8(uint8_t i) { WriteNoSizeCheck(&i, sizeof(i)); }
void SegmentBufferWriter::WriteBool(bool b) { Write8(b ? 1 : 0); }

void SegmentBufferWriter::WriteNoSizeCheck(const void* buffer, size_t size) {
  if (data_ == nullptr || pos_ == nullptr) {
    return;
  }

  if (end_ != nullptr && pos_ + size > end_) {
    return;
  }

  if (buffer != nullptr) {
    std::memcpy(pos_, buffer, size);
  }

  pos_ += size;
}

void BinaryWriteBuffer::WriteByteArray(const uint8_t* data, size_t size) {
  writer_.WriteUint32(static_cast<uint32_t>(size));
  writer_.WritePad(data, size);
}

void BinaryWriteBuffer::WriteBool(bool b) { writer_.WriteBool(b); }

void BinaryWriteBuffer::WriteFloat(float f) { writer_.WriteFloat(f); }

void BinaryWriteBuffer::WriteFloatArray(const float* data, size_t size) {
  writer_.WriteUint32(static_cast<uint32_t>(size));

  writer_.Write(data, size * sizeof(float));
}

void BinaryWriteBuffer::WriteInt32(int32_t i) { writer_.WriteInt32(i); }
void BinaryWriteBuffer::WriteUint32(uint32_t i) { writer_.WriteUint32(i); }
void BinaryWriteBuffer::WriteIntArray(const int32_t* data, size_t size) {
  writer_.WriteUint32(static_cast<uint32_t>(size));

  writer_.Write(data, size * sizeof(int32_t));
}

void BinaryWriteBuffer::WriteString(std::string_view str) {
  writer_.WriteString(str.data(), str.size());
}

void BinaryWriteBuffer::WriteColor(Color c) { writer_.WriteUint32(c); }
void BinaryWriteBuffer::WriteColorArray(const Color* data, size_t size) {
  writer_.WriteUint32(static_cast<uint32_t>(size));

  writer_.Write(data, size * sizeof(Color));
}

void BinaryWriteBuffer::WriteColor4f(const Color4f& c) {
  writer_.Write(&c, sizeof(Color4f));
}

void BinaryWriteBuffer::WriteColor4fArray(const Color4f* data, size_t size) {
  writer_.WriteUint32(static_cast<uint32_t>(size));

  writer_.Write(data, size * sizeof(Color4f));
}

void BinaryWriteBuffer::WritePoint(const Vec2& p) {
  writer_.WriteFloat(p.x);
  writer_.WriteFloat(p.y);
}

void BinaryWriteBuffer::WritePointArray(const Vec2* data, size_t size) {
  writer_.WriteUint32(static_cast<uint32_t>(size));

  writer_.Write(data, size * sizeof(Vec2));
}

void BinaryWriteBuffer::WriteMatrix(const Matrix& m) { writer_.WriteMatrix(m); }

void BinaryWriteBuffer::WriteRect(const Rect& rect) { writer_.WriteRect(rect); }

void BinaryWriteBuffer::WriteRRect(const RRect& rrect) {
  writer_.WriteRRect(rrect);
}

void BinaryWriteBuffer::WriteSampling(const SamplingOptions& sampling) {
  writer_.WriteSampling(sampling);
}

void BinaryWriteBuffer::WritePath(const Path& path) { writer_.WritePath(path); }

void BinaryWriteBuffer::WriteImage(const Image* image) {
  auto data = image_to_data(image, serial_proc_);

  if (!data) {
    return;
  }

  // we always encode image to unpremultiplied alpha format
  uint32_t flags = WriteBufferImageFlags::kUnpremul;

  this->WriteUint32(flags);
  this->WriteByteArray(data->Bytes(), data->Size());
}

void BinaryWriteBuffer::WriteTypeface(
    const std::shared_ptr<Typeface>& typeface) {
  // Write 32 bits (signed)
  //   0 -- empty font
  //  >0 -- index
  //  <0 -- custom (serial procs) ignore since we do not support custom serial

  if (typeface == nullptr || typeface_set_ == nullptr) {
    writer_.WriteInt32(0);
  } else {
    auto index = typeface_set_->AddTypeface(typeface);

    writer_.WriteInt32(index);
  }
}

void BinaryWriteBuffer::WritePaint(const Paint& paint) {
  FlatIntoMemory(paint, *this);
}

void BinaryWriteBuffer::WriteTextBlob(const TextBlob& blob) {
  FlatIntoMemory(blob, *this);
}

void BinaryWriteBuffer::WriteFlattenable(const Flattenable* flattenable) {
  if (flattenable == nullptr) {
    WriteInt32(0);
    return;
  }

  auto name = flattenable->ProcName();
  auto index = factory_set_->AddFactory(std::string(name));

  WriteInt32(index);

  writer_.Reserve(sizeof(uint32_t));

  auto offset = writer_.BytesWritten();

  flattenable->FlattenToBuffer(*this);

  auto size = writer_.BytesWritten() - offset;

  writer_.OverwriteAt(offset - sizeof(uint32_t), static_cast<uint32_t>(size));
}

}  // namespace skity
