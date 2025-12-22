// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <cassert>
#include <cstring>
#include <fstream>
#include <functional>
#include <mutex>
#include <skity/io/data.hpp>
#include <vector>

#include "src/base/mapping.hpp"

namespace skity {

static void skity_free_releaseproc(const void* ptr, void*) {
  std::free(const_cast<void*>(ptr));
}

Data::Data(const void* ptr, size_t size, ReleaseProc proc, void* context)
    : ptr_(ptr), size_(size), proc_(proc), context_(context) {}

Data::~Data() {
  if (proc_) {
    proc_(ptr_, context_);
  }
}

bool Data::WriteToFile(const char* filename) const {
  if (!ptr_ || !filename || !size_) {
    return false;
  }

  std::ofstream wf(filename, std::ios::out | std::ios::binary);
  if (!wf) {
    return false;
  }

  wf.write((const char*)ptr_, size_);

  return wf.good();
}

std::shared_ptr<Data> Data::PrivateNewWithCopy(const void* srcOrNull,
                                               size_t length) {
  if (0 == length) {
    return Data::MakeEmpty();
  }

  if (srcOrNull == nullptr) {
    return Data::MakeEmpty();
  }

  void* data = g_allocator ? g_allocator->Malloc(length) : std::malloc(length);
  if (data == nullptr) {
    return Data::MakeEmpty();
  }
  std::memcpy(data, srcOrNull, length);

  return Data::MakeFromMalloc(data, length);
}

static void file_mapping_releaseproc(const void* ptr, void* ctx) {
  delete reinterpret_cast<const FileMapping*>(ctx);
}

std::shared_ptr<Data> Data::MakeFromFileMapping(const char path[]) {
  std::unique_ptr<FileMapping> file_mapping = FileMapping::CreateReadOnly(path);
  if (!file_mapping || !file_mapping->IsValid() ||
      file_mapping->GetSize() <= 0) {
    return MakeEmpty();
  }
  FileMapping* file_mapping_pointer = file_mapping.release();
  return MakeWithProc(file_mapping_pointer->GetMapping(),
                      file_mapping_pointer->GetSize(), file_mapping_releaseproc,
                      file_mapping_pointer);
}

std::shared_ptr<Data> Data::MakeEmpty() {
  static std::shared_ptr<Data> empty;
  static std::once_flag flag;
  std::call_once(flag,
                 [] { empty.reset(new Data(nullptr, 0, nullptr, nullptr)); });
  return empty;
}

std::shared_ptr<Data> Data::MakeWithCopy(const void* data, size_t length) {
  if (!data || length == 0) {
    return MakeEmpty();
  }
  return PrivateNewWithCopy(data, length);
}

std::shared_ptr<Data> Data::MakeWithCString(const char cStr[]) {
  size_t size;
  if (nullptr == cStr) {
    cStr = "";
    size = 1;
  } else {
    size = std::strlen(cStr) + 1;
  }

  return MakeWithCopy(cStr, size);
}

std::shared_ptr<Data> Data::MakeFromFileName(const char path[]) {
  std::ifstream in_stream{path, std::ios::in | std::ios::binary};
  std::vector<uint8_t> raw_file_data(
      (std::istreambuf_iterator<char>(in_stream)),
      std::istreambuf_iterator<char>());

  return MakeWithCopy(raw_file_data.data(), raw_file_data.size());
}

std::shared_ptr<Data> Data::MakeWithProc(const void* ptr, size_t length,
                                         ReleaseProc proc, void* ctx) {
  return std::shared_ptr<Data>(new Data(ptr, length, proc, ctx));
}

std::shared_ptr<Data> Data::MakeFromMalloc(const void* data, size_t length) {
  return std::shared_ptr<Data>(
      new Data(data, length, skity_free_releaseproc, nullptr));
}

DataAllocator* Data::g_allocator = nullptr;
void Data::SetAllocatorForTest(DataAllocator* allocator) {
  g_allocator = allocator;
}

}  // namespace skity
