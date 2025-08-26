// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#define WUFFS_IMPLEMENTATION  // implementation in this source file

// after wuffs macros
#include "src/codec/wuffs/wuffs_module.hpp"

#include <cstring>
#include <limits>

#include "src/codec/data_stream.hpp"

namespace skity {

WuffsBuffer::WuffsBuffer() {
  buffer = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(data.data(), kWuffsBufferSize),
      wuffs_base__empty_io_buffer_meta());
}

WuffsBuffer::WuffsBuffer(const WuffsBuffer& other) {
  buffer = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(data.data(), kWuffsBufferSize),
      other.buffer.meta);

  buffer.meta = other.buffer.meta;

  std::memcpy(data.data(), other.data.data(), kWuffsBufferSize);
}

WuffsBuffer::WuffsBuffer(WuffsBuffer&& other) {
  buffer = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(data.data(), kWuffsBufferSize),
      other.buffer.meta);

  buffer.meta = other.buffer.meta;

  std::memcpy(data.data(), other.data.data(), kWuffsBufferSize);

  other.buffer.meta = wuffs_base__empty_io_buffer_meta();
}

WuffsBuffer& WuffsBuffer::operator=(const WuffsBuffer& other) {
  if (this != &other) {
    buffer = wuffs_base__make_io_buffer(
        wuffs_base__make_slice_u8(data.data(), kWuffsBufferSize),
        other.buffer.meta);

    buffer.meta = other.buffer.meta;

    std::memcpy(data.data(), other.data.data(), kWuffsBufferSize);
  }

  return *this;
}

WuffsBuffer& WuffsBuffer::operator=(WuffsBuffer&& other) {
  buffer = wuffs_base__make_io_buffer(
      wuffs_base__make_slice_u8(data.data(), kWuffsBufferSize),
      other.buffer.meta);

  buffer.meta = other.buffer.meta;

  std::memcpy(data.data(), other.data.data(), kWuffsBufferSize);

  other.buffer.meta = wuffs_base__empty_io_buffer_meta();

  return *this;
}

WuffsBuffer::~WuffsBuffer() {
  buffer.meta = wuffs_base__empty_io_buffer_meta();
}

bool WuffsBuffer::FillBuffer(DataStream* stream) {
  if (!stream) {
    return false;
  }

  buffer.compact();

  auto read_size = stream->Read(buffer.data.ptr + buffer.meta.wi,
                                buffer.data.len - buffer.meta.wi);
  buffer.meta.wi += read_size;

  // hard-code set to false.
  buffer.meta.closed = false;

  return read_size > 0;
}

bool WuffsBuffer::SeekBuffer(DataStream* stream, size_t pos) {
  if (!stream) {
    return false;
  }

  if ((pos >= buffer.meta.pos) && (pos - buffer.meta.pos <= buffer.meta.wi)) {
    buffer.meta.ri = pos - buffer.meta.pos;
    return true;
  }

  if (pos > std::numeric_limits<size_t>::max() || !stream->Seek(pos)) {
    return false;
  }

  buffer.meta.wi = 0;
  buffer.meta.ri = 0;
  buffer.meta.pos = pos;
  buffer.meta.closed = false;

  return true;
}

CodecDisposalMethod WuffsToCodecDisposalMethod(
    wuffs_base__animation_disposal method) {
  switch (method) {
    case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_BACKGROUND:
      return CodecDisposalMethod::RestoreBGColor;
    case WUFFS_BASE__ANIMATION_DISPOSAL__RESTORE_PREVIOUS:
      return CodecDisposalMethod::RestorePrevious;
    default:
      return CodecDisposalMethod::Keep;
  }
}

}  // namespace skity
