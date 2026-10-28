// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_IO_DATA_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_IO_DATA_HPP

#include <skity/c/skity_data.h>

#include <cstddef>
#include <skity/base.hpp>

namespace skity {

/**
 * @class Data
 *
 * Immutable byte buffer wrapper (legacy skity::Data).
 *
 */
class Data : public detail::OwnHandle<skity_data, skity_data_destroy> {
 public:
  /** Copy the first length bytes of data into a new buffer. */
  static Data MakeWithCopy(const void* data, size_t length) {
    return Data(skity_data_make_with_copy(data, length));
  }

  /**
   * Wrap ptr without copying; proc (if set) runs with context once
   * the buffer's last reference goes away — the place to unlock/free the
   * source. ptr must stay valid until then.
   */
  static Data MakeWithProc(const void* ptr, size_t length,
                           skity_data_release_proc proc, void* context) {
    return Data(skity_data_make_with_proc(ptr, length, proc, context));
  }

  /** Load a whole file into memory; empty wrapper on failure. */
  static Data MakeFromFile(const char* path) {
    return Data(skity_data_make_from_file(path));
  }

  /** Shared, always-valid empty buffer. */
  static Data MakeEmpty() { return Data(skity_data_make_empty()); }

  /** Number of bytes stored. */
  size_t GetSize() const { return skity_data_get_size(get()); }

  /**
   * Read-only pointer to the bytes; stable for the handle's lifetime, NULL
   * for an empty buffer.
   */
  const void* GetData() const { return skity_data_get_data(get()); }

 private:
  explicit Data(skity_data h) : OwnHandle(h) {}
  friend class Typeface;
};

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_IO_DATA_HPP
