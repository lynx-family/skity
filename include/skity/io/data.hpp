// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_IO_DATA_HPP
#define INCLUDE_SKITY_IO_DATA_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <skity/macros.hpp>

namespace skity {

/**
 * This class holds an immutable data buffer. Not only is the data immutable,
 * but the actual ptr that is returned is guaranteed to always be the same for
 * the life of this instance.
 *
 */
class SKITY_API Data final {
 public:
  ~Data();
  Data& operator=(const Data&) = delete;
  /**
   * @brief Returns the number of bytes stored.
   *
   * @return the number of bytes stored.
   */
  size_t Size() const { return size_; }
  bool IsEmpty() const { return 0 == size_; }
  const void* RawData() const { return ptr_; }
  const uint8_t* Bytes() const {
    return reinterpret_cast<const uint8_t*>(ptr_);
  }

  bool WriteToFile(const char* filename) const;

  /**
   * Function that, if provided, will be called when the SkData goes out
   *  of scope, allowing for custom allocation/freeing of the data's contents.
   */
  typedef void (*ReleaseProc)(const void* ptr, void* context);

  /**
   * Create a new dataref by copying the specified data
   */
  static std::shared_ptr<Data> MakeWithCopy(const void* data, size_t length);

  /**
   * Create a new dataref by copying the specified c-string. The returned
   * Data will have size() equal to strlen(cstr) + 1. If cstr is NULL, it will
   * be treated the same as "".
   */
  static std::shared_ptr<Data> MakeWithCString(const char cstr[]);

  /**
   * Create a new dataref from a pointer allocated by malloc. The Data object
   * takes ownership of that allocation, and will handling calling free.
   */
  static std::shared_ptr<Data> MakeFromFileName(const char path[]);

  /**
   * Create a new dataref, taking the ptr as is, and using the
   * releaseproc to free it. The proc may be NULL.
   */
  static std::shared_ptr<Data> MakeWithProc(const void* ptr, size_t length,
                                            ReleaseProc proc, void* ctx);

  /**
   * Create a new dataref from a pointer allocated by malloc. The Data object
   * takes ownership of that allocation, and will handling calling free.
   */
  static std::shared_ptr<Data> MakeFromMalloc(const void* data, size_t length);

  /**
   * Create a new dataref from a file by memory mapping. The Data object
   * takes ownership of that allocation, and will handling calling free.
   */
  static std::shared_ptr<Data> MakeFromFileMapping(const char path[]);

  /**
   * Returns a new empty dataref (or a reference to a shared empty dataref).
   *
   * @return std::shared_ptr<Data>
   */
  static std::shared_ptr<Data> MakeEmpty();

 private:
  static std::shared_ptr<Data> PrivateNewWithCopy(const void* srcOrNull,
                                                  size_t length);

 private:
  Data(const void* ptr, size_t size, ReleaseProc proc, void* context);
  const void* ptr_;
  size_t size_;
  ReleaseProc proc_;
  void* context_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_IO_DATA_HPP
