// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_data.h>

#include <skity/io/data.hpp>

#include "handle.hpp"

namespace {

skity::Data* data_of(skity_data handle) {
  auto* w = skity::capi::resolve<skity_data_s>(handle, SKITY_OBJECT_TYPE_DATA);
  return w ? static_cast<skity::Data*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_data skity_data_make_with_copy(const void* data, size_t length) {
  auto d = skity::Data::MakeWithCopy(data, length);
  if (d == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_data_s>(
      SKITY_OBJECT_TYPE_DATA, SKITY_HANDLE_OWNING, std::move(d));
}

skity_data skity_data_make_from_file(const char* path) {
  if (path == nullptr) return nullptr;
  auto d = skity::Data::MakeFromFileName(path);
  if (d == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_data_s>(
      SKITY_OBJECT_TYPE_DATA, SKITY_HANDLE_OWNING, std::move(d));
}

skity_data skity_data_make_empty(void) {
  auto d = skity::Data::MakeEmpty();
  if (d == nullptr) return nullptr;
  return skity::capi::alloc_handle<skity_data_s>(
      SKITY_OBJECT_TYPE_DATA, SKITY_HANDLE_OWNING, std::move(d));
}

void skity_data_destroy(skity_data data) {
  skity::capi::destroy_handle<skity_data_s>(data, SKITY_OBJECT_TYPE_DATA);
}

size_t skity_data_get_size(skity_data data) {
  auto* d = data_of(data);
  return d ? d->Size() : 0u;
}

const void* skity_data_get_data(skity_data data) {
  auto* d = data_of(data);
  return d ? d->RawData() : nullptr;
}

}  // extern "C"
