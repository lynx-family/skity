/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/ports/win/typeface_win.hpp"

// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include "src/base/platform/win/handle_result.hpp"
#include "src/base/platform/win/str_conversion.hpp"
#include "src/text/ports/win/scaler_context_win.hpp"
// clang-format on

#ifdef GetGlyphIndices
#undef GetGlyphIndices
#endif

#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "src/logging.hpp"

namespace skity {
namespace {

static constexpr FourByteTag kDWriteFactoryID =
    SetFourByteTag('d', 'w', 'r', 't');

/** Reverse all 4 bytes in a 32bit value.
  e.g. 0x12345678 -> 0x78563412
*/
static constexpr uint32_t EndianSwap32(uint32_t value) {
  return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
         ((value & 0xFF0000) >> 8) | (value >> 24);
}

int16_t ReadBigEndianInt16(const uint8_t* data) {
  return static_cast<int16_t>((static_cast<uint16_t>(data[0]) << 8) |
                              static_cast<uint16_t>(data[1]));
}

uint16_t ReadBigEndianUInt16(const uint8_t* data) {
  return (static_cast<uint16_t>(data[0]) << 8) | static_cast<uint16_t>(data[1]);
}

uint32_t ReadBigEndianUInt32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) |
         (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

bool RangeInBounds(size_t offset, size_t length, size_t total_size) {
  return offset <= total_size && length <= total_size - offset;
}

bool IsSfntFontType(uint32_t font_type) {
  return font_type == SetFourByteTag(0, 1, 0, 0) ||
         font_type == SetFourByteTag('t', 'r', 'u', 'e') ||
         font_type == SetFourByteTag('O', 'T', 'T', 'O') ||
         font_type == SetFourByteTag('t', 'y', 'p', '1');
}

bool GetSfntOffset(const std::shared_ptr<Data>& data, UINT32 face_index,
                   size_t* sfnt_offset) {
  static constexpr uint32_t kTtcTag = SetFourByteTag('t', 't', 'c', 'f');
  static constexpr size_t kSfntHeaderSize = 12;
  static constexpr size_t kTtcFaceOffsetsStart = 12;

  if (!data || !sfnt_offset ||
      !RangeInBounds(0, kSfntHeaderSize, data->Size())) {
    return false;
  }

  const uint8_t* bytes = data->Bytes();
  const uint32_t font_type = ReadBigEndianUInt32(bytes);
  if (font_type == kTtcTag) {
    const uint32_t face_count = ReadBigEndianUInt32(bytes + 8);
    if (face_index >= face_count) {
      return false;
    }

    const size_t face_offset_entry =
        kTtcFaceOffsetsStart + static_cast<size_t>(face_index) * 4;
    if (!RangeInBounds(face_offset_entry, 4, data->Size())) {
      return false;
    }

    const size_t face_offset =
        static_cast<size_t>(ReadBigEndianUInt32(bytes + face_offset_entry));
    if (!RangeInBounds(face_offset, kSfntHeaderSize, data->Size())) {
      return false;
    }

    const uint32_t face_type = ReadBigEndianUInt32(bytes + face_offset);
    if (!IsSfntFontType(face_type)) {
      return false;
    }

    *sfnt_offset = face_offset;
    return true;
  }

  if (face_index != 0 || !IsSfntFontType(font_type)) {
    return false;
  }

  *sfnt_offset = 0;
  return true;
}

int GetSfntTableTags(const std::shared_ptr<Data>& data, UINT32 face_index,
                     FontTableTag tags[]) {
  static constexpr size_t kSfntHeaderSize = 12;
  static constexpr size_t kSfntTableDirectoryEntrySize = 16;
  static constexpr size_t kSfntNumTablesOffset = 4;

  size_t sfnt_offset = 0;
  if (!GetSfntOffset(data, face_index, &sfnt_offset)) {
    return 0;
  }

  const uint8_t* bytes = data->Bytes();
  const uint16_t table_count =
      ReadBigEndianUInt16(bytes + sfnt_offset + kSfntNumTablesOffset);
  const size_t table_directory_offset = sfnt_offset + kSfntHeaderSize;
  const size_t table_directory_size =
      static_cast<size_t>(table_count) * kSfntTableDirectoryEntrySize;
  if (!RangeInBounds(table_directory_offset, table_directory_size,
                     data->Size())) {
    return 0;
  }

  if (tags) {
    for (uint16_t i = 0; i < table_count; ++i) {
      const size_t table_offset =
          table_directory_offset +
          static_cast<size_t>(i) * kSfntTableDirectoryEntrySize;
      tags[i] = ReadBigEndianUInt32(bytes + table_offset);
    }
  }
  return table_count;
}

#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
FourByteTag DWriteAxisTagToFourByteTag(DWRITE_FONT_AXIS_TAG tag) {
  return EndianSwap32(static_cast<uint32_t>(tag));
}

bool IsDWriteVariableAxis(IDWriteFontResource* font_resource,
                          UINT32 axis_index) {
  return (font_resource->GetFontAxisAttributes(axis_index) &
          DWRITE_FONT_AXIS_ATTRIBUTES_VARIABLE) != 0;
}

VariationPosition GetDWriteVariationDesignPosition(
    IDWriteFontFace5* font_face5) {
  VariationPosition position;
  if (!font_face5 || !font_face5->HasVariations()) {
    return position;
  }

  ScopedComPtr<IDWriteFontResource> font_resource;
  if (FAILED(font_face5->GetFontResource(&font_resource))) {
    return {};
  }

  UINT32 axis_count = font_face5->GetFontAxisValueCount();
  std::vector<DWRITE_FONT_AXIS_VALUE> axis_values(axis_count);
  if (axis_count == 0 ||
      FAILED(font_face5->GetFontAxisValues(axis_values.data(), axis_count))) {
    return {};
  }

  for (UINT32 i = 0; i < axis_count; i++) {
    if (!IsDWriteVariableAxis(font_resource.get(), i)) {
      continue;
    }
    position.AddCoordinate(DWriteAxisTagToFourByteTag(axis_values[i].axisTag),
                           axis_values[i].value);
  }
  return position;
}

std::vector<VariationAxis> GetDWriteVariationDesignParameters(
    IDWriteFontFace5* font_face5) {
  if (!font_face5 || !font_face5->HasVariations()) {
    return {};
  }

  ScopedComPtr<IDWriteFontResource> font_resource;
  if (FAILED(font_face5->GetFontResource(&font_resource))) {
    return {};
  }

  UINT32 axis_count = font_face5->GetFontAxisValueCount();
  if (axis_count == 0) {
    return {};
  }

  std::vector<DWRITE_FONT_AXIS_RANGE> axis_ranges(axis_count);
  if (FAILED(
          font_resource->GetFontAxisRanges(axis_ranges.data(), axis_count))) {
    return {};
  }

  std::vector<DWRITE_FONT_AXIS_VALUE> default_values(axis_count);
  if (FAILED(font_resource->GetDefaultFontAxisValues(default_values.data(),
                                                     axis_count))) {
    return {};
  }

  std::vector<VariationAxis> axes;
  axes.reserve(axis_count);
  for (UINT32 i = 0; i < axis_count; i++) {
    if (!IsDWriteVariableAxis(font_resource.get(), i)) {
      continue;
    }
    const auto attributes = font_resource->GetFontAxisAttributes(i);
    axes.emplace_back(DWriteAxisTagToFourByteTag(default_values[i].axisTag),
                      axis_ranges[i].minValue, default_values[i].value,
                      axis_ranges[i].maxValue,
                      (attributes & DWRITE_FONT_AXIS_ATTRIBUTES_HIDDEN) != 0);
  }
  return axes;
}

ScopedComPtr<IDWriteFontFace> MakeDWriteVariationFontFace(
    IDWriteFontFace5* font_face5, const FontArguments& args) {
  if (!font_face5 || !font_face5->HasVariations()) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  UINT32 axis_count = font_face5->GetFontAxisValueCount();
  if (axis_count == 0) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  std::vector<DWRITE_FONT_AXIS_VALUE> axis_values(axis_count);
  if (FAILED(font_face5->GetFontAxisValues(axis_values.data(), axis_count))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  for (const auto& coordinate :
       args.GetVariationDesignPosition().GetCoordinates()) {
    for (auto& axis_value : axis_values) {
      if (DWriteAxisTagToFourByteTag(axis_value.axisTag) == coordinate.axis) {
        axis_value.value = coordinate.value;
        break;
      }
    }
  }

  ScopedComPtr<IDWriteFontResource> font_resource;
  if (FAILED(font_face5->GetFontResource(&font_resource))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  ScopedComPtr<IDWriteFontFace5> new_font_face5;
  if (FAILED(font_resource->CreateFontFace(font_face5->GetSimulations(),
                                           axis_values.data(), axis_count,
                                           &new_font_face5))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  ScopedComPtr<IDWriteFontFace> new_font_face;
  if (FAILED(new_font_face5->QueryInterface(&new_font_face))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }
  return new_font_face;
}

ScopedComPtr<IDWriteFontFace> MakeDWriteDefaultVariationFontFace(
    IDWriteFontFace* font_face) {
  if (!font_face) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  ScopedComPtr<IDWriteFontFace5> font_face5;
  if (FAILED(font_face->QueryInterface(&font_face5)) ||
      !font_face5->HasVariations()) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  UINT32 axis_count = font_face5->GetFontAxisValueCount();
  if (axis_count == 0) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  ScopedComPtr<IDWriteFontResource> font_resource;
  if (FAILED(font_face5->GetFontResource(&font_resource))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  std::vector<DWRITE_FONT_AXIS_VALUE> axis_values(axis_count);
  if (FAILED(font_resource->GetDefaultFontAxisValues(axis_values.data(),
                                                     axis_count))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  ScopedComPtr<IDWriteFontFace5> default_font_face5;
  if (FAILED(font_resource->CreateFontFace(font_face->GetSimulations(),
                                           axis_values.data(), axis_count,
                                           &default_font_face5))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }

  ScopedComPtr<IDWriteFontFace> default_font_face;
  if (FAILED(default_font_face5->QueryInterface(&default_font_face))) {
    return ScopedComPtr<IDWriteFontFace>(nullptr);
  }
  return default_font_face;
}
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__

// Korean fonts Gulim, Dotum, Batang, Gungsuh have bitmap strikes that get
// artifically emboldened by Windows without antialiasing. Korean users prefer
// these over the synthetic boldening performed by Skia. So let's make an
// exception for fonts with bitmap strikes and allow passing through Windows
// simulations for those, until Skia provides more control over simulations in

}  // namespace

bool HasBitmapStrikes(const ScopedComPtr<IDWriteFont>& font) {
  ScopedComPtr<IDWriteFontFace> fontFace;
  HRB(font->CreateFontFace(&fontFace));

  AutoDWriteTable ebdtTable(fontFace.get(),
                            EndianSwap32(SetFourByteTag('E', 'B', 'D', 'T')));
  return ebdtTable.exists;
}

template <typename T, typename U>
bool SameCOMObject(T* lhs, U* rhs) {
  if (!lhs || !rhs) {
    return lhs == rhs;
  }

  ScopedComPtr<IUnknown> lhsUnknown;
  if (FAILED(lhs->QueryInterface(&lhsUnknown))) {
    return false;
  }

  ScopedComPtr<IUnknown> rhsUnknown;
  if (FAILED(rhs->QueryInterface(&rhsUnknown))) {
    return false;
  }

  return lhsUnknown.get() == rhsUnknown.get();
}

bool GetDWriteFontFiles(IDWriteFontFace* font_face,
                        std::vector<ScopedComPtr<IDWriteFontFile>>& files) {
  files.clear();

  uint32_t number_of_files = 0;
  if (FAILED(font_face->GetFiles(&number_of_files, nullptr))) {
    return false;
  }

  files.resize(number_of_files);
  if (files.empty()) {
    return true;
  }

  return SUCCEEDED(font_face->GetFiles(
      &number_of_files, reinterpret_cast<IDWriteFontFile**>(files.data())));
}

bool DWriteFontFileEqual(IDWriteFontFile* lhs, IDWriteFontFile* rhs) {
  if (SameCOMObject(lhs, rhs)) {
    return true;
  }

  if (!lhs || !rhs) {
    return false;
  }

  ScopedComPtr<IDWriteFontFileLoader> lhsLoader;
  ScopedComPtr<IDWriteFontFileLoader> rhsLoader;
  if (FAILED(lhs->GetLoader(&lhsLoader)) ||
      FAILED(rhs->GetLoader(&rhsLoader))) {
    return false;
  }

  if (!SameCOMObject(lhsLoader.get(), rhsLoader.get())) {
    return false;
  }

  const void* lhsKey = nullptr;
  const void* rhsKey = nullptr;
  UINT32 lhsKeySize = 0;
  UINT32 rhsKeySize = 0;
  lhs->GetReferenceKey(&lhsKey, &lhsKeySize);
  rhs->GetReferenceKey(&rhsKey, &rhsKeySize);

  if (lhsKeySize != rhsKeySize) {
    return false;
  }

  if (lhsKeySize == 0) {
    return true;
  }

  return lhsKey && rhsKey && std::memcmp(lhsKey, rhsKey, lhsKeySize) == 0;
}

bool DWriteFontFaceEqualByFiles(IDWriteFontFace* lhs, IDWriteFontFace* rhs) {
  if (lhs->GetIndex() != rhs->GetIndex() ||
      lhs->GetSimulations() != rhs->GetSimulations()) {
    return false;
  }

  std::vector<ScopedComPtr<IDWriteFontFile>> lhsFiles;
  std::vector<ScopedComPtr<IDWriteFontFile>> rhsFiles;
  if (!GetDWriteFontFiles(lhs, lhsFiles) ||
      !GetDWriteFontFiles(rhs, rhsFiles)) {
    return false;
  }

  if (lhsFiles.size() != rhsFiles.size()) {
    return false;
  }

  for (size_t index = 0; index < lhsFiles.size(); index++) {
    if (!DWriteFontFileEqual(lhsFiles[index].get(), rhsFiles[index].get())) {
      return false;
    }
  }

  return true;
}

bool DWriteFontFaceEqual(IDWriteFontFace* lhs, IDWriteFontFace* rhs) {
  if (SameCOMObject(lhs, rhs)) {
    return true;
  }

  if (!lhs || !rhs) {
    return false;
  }

  // IDWriteFontFace5::Equals accounts for newer face identity such as variation
  // axis values. Older DirectWrite versions can only compare file identity.
#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
  ScopedComPtr<IDWriteFontFace5> lhsFace5;
  if (SUCCEEDED(lhs->QueryInterface(&lhsFace5))) {
    return lhsFace5->Equals(rhs);
  }

  ScopedComPtr<IDWriteFontFace5> rhsFace5;
  if (SUCCEEDED(rhs->QueryInterface(&rhsFace5))) {
    return rhsFace5->Equals(lhs);
  }
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__

  return DWriteFontFaceEqualByFiles(lhs, rhs);
}

namespace {

class DataFontFileStream : public IDWriteFontFileStream {
 public:
  explicit DataFontFileStream(std::shared_ptr<Data> data)
      : ref_count_(1), data_(std::move(data)) {}

  // IUnknown methods
  SK_STDMETHODIMP QueryInterface(IID const& riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == __uuidof(IDWriteFontFileStream)) {
      *object = this;
      AddRef();
      return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG new_count = InterlockedDecrement(&ref_count_);
    if (new_count == 0) {
      delete this;
    }
    return new_count;
  }

  // IDWriteFontFileStream methods
  SK_STDMETHODIMP ReadFileFragment(const void** fragment_start,
                                   UINT64 file_offset, UINT64 fragment_size,
                                   void** fragment_context) override {
    if (!fragment_start || !fragment_context || !data_) {
      return E_POINTER;
    }

    *fragment_start = nullptr;
    *fragment_context = nullptr;

    UINT64 file_size = static_cast<UINT64>(data_->Size());
    if (file_offset > file_size || fragment_size > file_size - file_offset) {
      return E_FAIL;
    }

    *fragment_start = data_->Bytes() + static_cast<size_t>(file_offset);
    return S_OK;
  }

  SK_STDMETHODIMP_(void) ReleaseFileFragment(void*) override {}

  SK_STDMETHODIMP GetFileSize(UINT64* file_size) override {
    if (!file_size || !data_) {
      return E_POINTER;
    }

    *file_size = static_cast<UINT64>(data_->Size());
    return S_OK;
  }

  SK_STDMETHODIMP GetLastWriteTime(UINT64* last_write_time) override {
    if (!last_write_time) {
      return E_POINTER;
    }

    *last_write_time = 0;
    return E_NOTIMPL;
  }

 private:
  ~DataFontFileStream() = default;

  ULONG ref_count_;
  std::shared_ptr<Data> data_;
};

class DataFontFileLoader : public IDWriteFontFileLoader {
 public:
  explicit DataFontFileLoader(std::shared_ptr<Data> data)
      : ref_count_(1), data_(std::move(data)) {}

  // IUnknown methods
  SK_STDMETHODIMP QueryInterface(IID const& riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == __uuidof(IDWriteFontFileLoader)) {
      *object = this;
      AddRef();
      return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG new_count = InterlockedDecrement(&ref_count_);
    if (new_count == 0) {
      delete this;
    }
    return new_count;
  }

  // IDWriteFontFileLoader methods
  SK_STDMETHODIMP CreateStreamFromKey(const void*, UINT32,
                                      IDWriteFontFileStream** stream) override {
    if (!stream || !data_) {
      return E_POINTER;
    }

    *stream = new DataFontFileStream(data_);
    return *stream ? S_OK : E_OUTOFMEMORY;
  }

 private:
  ~DataFontFileLoader() = default;

  ULONG ref_count_;
  std::shared_ptr<Data> data_;
};

class DataFontFileEnumerator : public IDWriteFontFileEnumerator {
 public:
  DataFontFileEnumerator(IDWriteFactory* factory,
                         IDWriteFontFileLoader* font_file_loader)
      : ref_count_(1),
        factory_(RefComPtr(factory)),
        font_file_loader_(RefComPtr(font_file_loader)),
        has_next_(true) {}

  SK_STDMETHODIMP QueryInterface(IID const& riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == __uuidof(IDWriteFontFileEnumerator)) {
      *object = this;
      AddRef();
      return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG new_count = InterlockedDecrement(&ref_count_);
    if (new_count == 0) {
      delete this;
    }
    return new_count;
  }

  SK_STDMETHODIMP MoveNext(BOOL* has_current_file) override {
    if (!has_current_file) {
      return E_POINTER;
    }

    *has_current_file = FALSE;
    if (!has_next_) {
      return S_OK;
    }
    has_next_ = false;

    UINT32 font_file_reference_key = 0;
    HR(factory_->CreateCustomFontFileReference(
        &font_file_reference_key, sizeof(font_file_reference_key),
        font_file_loader_.get(), &current_file_));
    *has_current_file = TRUE;
    return S_OK;
  }

  SK_STDMETHODIMP GetCurrentFontFile(IDWriteFontFile** font_file) override {
    if (!font_file) {
      return E_POINTER;
    }

    if (!current_file_) {
      *font_file = nullptr;
      return E_FAIL;
    }

    *font_file = RefComPtr(current_file_.get());
    return S_OK;
  }

 private:
  ~DataFontFileEnumerator() = default;

  ULONG ref_count_;
  ScopedComPtr<IDWriteFactory> factory_;
  ScopedComPtr<IDWriteFontFile> current_file_;
  ScopedComPtr<IDWriteFontFileLoader> font_file_loader_;
  bool has_next_;
};

class DataFontCollectionLoader : public IDWriteFontCollectionLoader {
 public:
  explicit DataFontCollectionLoader(IDWriteFontFileLoader* font_file_loader)
      : ref_count_(1), font_file_loader_(RefComPtr(font_file_loader)) {}

  SK_STDMETHODIMP QueryInterface(IID const& riid, void** object) override {
    if (!object) {
      return E_POINTER;
    }

    if (riid == IID_IUnknown || riid == __uuidof(IDWriteFontCollectionLoader)) {
      *object = this;
      AddRef();
      return S_OK;
    }

    *object = nullptr;
    return E_NOINTERFACE;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG new_count = InterlockedDecrement(&ref_count_);
    if (new_count == 0) {
      delete this;
    }
    return new_count;
  }

  SK_STDMETHODIMP CreateEnumeratorFromKey(
      IDWriteFactory* factory, const void*, UINT32,
      IDWriteFontFileEnumerator** font_file_enumerator) override {
    if (!factory || !font_file_enumerator) {
      return E_POINTER;
    }

    *font_file_enumerator =
        new DataFontFileEnumerator(factory, font_file_loader_.get());
    return *font_file_enumerator ? S_OK : E_OUTOFMEMORY;
  }

 private:
  ~DataFontCollectionLoader() = default;

  ULONG ref_count_;
  ScopedComPtr<IDWriteFontFileLoader> font_file_loader_;
};

class RegisteredFontFileLoader {
 public:
  static std::shared_ptr<RegisteredFontFileLoader> Make(
      IDWriteFactory* factory, IDWriteFontFileLoader* loader) {
    if (!factory || !loader ||
        FAILED(factory->RegisterFontFileLoader(loader))) {
      return nullptr;
    }

    return std::shared_ptr<RegisteredFontFileLoader>(
        new RegisteredFontFileLoader(factory, loader));
  }

  ~RegisteredFontFileLoader() {
    if (factory_ && loader_) {
      factory_->UnregisterFontFileLoader(loader_.get());
    }
  }

 private:
  RegisteredFontFileLoader(IDWriteFactory* factory,
                           IDWriteFontFileLoader* loader)
      : factory_(RefComPtr(factory)), loader_(RefComPtr(loader)) {}

  ScopedComPtr<IDWriteFactory> factory_;
  ScopedComPtr<IDWriteFontFileLoader> loader_;
};

class RegisteredFontCollectionLoader {
 public:
  static std::shared_ptr<RegisteredFontCollectionLoader> Make(
      IDWriteFactory* factory, IDWriteFontCollectionLoader* loader) {
    if (!factory || !loader ||
        FAILED(factory->RegisterFontCollectionLoader(loader))) {
      return nullptr;
    }

    return std::shared_ptr<RegisteredFontCollectionLoader>(
        new RegisteredFontCollectionLoader(factory, loader));
  }

  ~RegisteredFontCollectionLoader() {
    if (factory_ && loader_) {
      factory_->UnregisterFontCollectionLoader(loader_.get());
    }
  }

 private:
  RegisteredFontCollectionLoader(IDWriteFactory* factory,
                                 IDWriteFontCollectionLoader* loader)
      : factory_(RefComPtr(factory)), loader_(RefComPtr(loader)) {}

  ScopedComPtr<IDWriteFactory> factory_;
  ScopedComPtr<IDWriteFontCollectionLoader> loader_;
};

}  // namespace

// Iterate calls to GetFirstMatchingFont incrementally removing bold or italic
// styling that can trigger the simulations. Implementing it this way gets us a
// IDWriteFont that can be used as before and has the correct information on its
// own style. Stripping simulations from IDWriteFontFace is possible via
// IDWriteFontList1, IDWriteFontFaceReference and CreateFontFace, but this way
// we won't have a matching IDWriteFont which is still used in get_style().
HRESULT FirstMatchingFontWithoutSimulations(
    const ScopedComPtr<IDWriteFontFamily>& family, DWriteStyle dwStyle,
    ScopedComPtr<IDWriteFont>& font) {
  bool noSimulations = false;
  while (!noSimulations) {
    ScopedComPtr<IDWriteFont> searchFont;
    HR(family->GetFirstMatchingFont(dwStyle.weight, dwStyle.width,
                                    dwStyle.slant, &searchFont));
    DWRITE_FONT_SIMULATIONS simulations = searchFont->GetSimulations();
    // If we still get simulations even though we're not asking for bold or
    // italic, we can't help it and exit the loop.

    noSimulations = simulations == DWRITE_FONT_SIMULATIONS_NONE ||
                    (dwStyle.weight == DWRITE_FONT_WEIGHT_REGULAR &&
                     dwStyle.slant == DWRITE_FONT_STYLE_NORMAL) ||
                    HasBitmapStrikes(searchFont);

    if (noSimulations) {
      font = std::move(searchFont);
      break;
    }
    if (simulations & DWRITE_FONT_SIMULATIONS_BOLD) {
      dwStyle.weight = DWRITE_FONT_WEIGHT_REGULAR;
      continue;
    }
    if (simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
      dwStyle.slant = DWRITE_FONT_STYLE_NORMAL;
      continue;
    }
  }
  return S_OK;
}

FontStyle::Slant FontSlantFromDWrite(DWRITE_FONT_STYLE slant) {
  switch (slant) {
    case DWRITE_FONT_STYLE_ITALIC:
      return FontStyle::kItalic_Slant;
    case DWRITE_FONT_STYLE_OBLIQUE:
      return FontStyle::kOblique_Slant;
    case DWRITE_FONT_STYLE_NORMAL:
    default:
      return FontStyle::kUpright_Slant;
  }
}

FontStyle FontStyleFromDWriteFont(IDWriteFont* font) {
  return FontStyle(static_cast<int>(font->GetWeight()),
                   static_cast<int>(font->GetStretch()),
                   FontSlantFromDWrite(font->GetStyle()));
}

namespace {

int FontWidthFromVariationWidth(float width) {
  if (width <= 50.0f) {
    return 1;
  }
  if (width <= 62.5f) {
    return 2;
  }
  if (width <= 75.0f) {
    return 3;
  }
  if (width <= 87.5f) {
    return 4;
  }
  if (width <= 100.0f) {
    return 5;
  }
  if (width <= 112.5f) {
    return 6;
  }
  if (width <= 125.0f) {
    return 7;
  }
  if (width <= 150.0f) {
    return 8;
  }
  return 9;
}

FontStyle ApplyVariationStyle(IDWriteFontFace* font_face,
                              const FontStyle& style) {
#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
  ScopedComPtr<IDWriteFontFace5> font_face5;
  if (!font_face || FAILED(font_face->QueryInterface(&font_face5)) ||
      !font_face5->HasVariations()) {
    return style;
  }

  UINT32 axis_count = font_face5->GetFontAxisValueCount();
  if (axis_count == 0) {
    return style;
  }

  std::vector<DWRITE_FONT_AXIS_VALUE> axis_values(axis_count);
  if (FAILED(font_face5->GetFontAxisValues(axis_values.data(), axis_count))) {
    return style;
  }

  int weight = style.weight();
  int width = style.width();
  FontStyle::Slant slant = style.slant();
  for (const auto& axis_value : axis_values) {
    FourByteTag axis = DWriteAxisTagToFourByteTag(axis_value.axisTag);
    if (axis == SetFourByteTag('w', 'g', 'h', 't')) {
      weight = static_cast<int>(std::lround(axis_value.value));
    } else if (axis == SetFourByteTag('w', 'd', 't', 'h')) {
      width = FontWidthFromVariationWidth(axis_value.value);
    } else if (axis == SetFourByteTag('i', 't', 'a', 'l') &&
               axis_value.value > 0.0f) {
      slant = FontStyle::kItalic_Slant;
    } else if (axis == SetFourByteTag('s', 'l', 'n', 't') &&
               axis_value.value != 0.0f) {
      slant = FontStyle::kOblique_Slant;
    }
  }

  return FontStyle(weight, width, slant);
#else
  (void)font_face;
  return style;
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__
}

std::optional<FontStyle> FontStyleFromOS2Table(IDWriteFontFace* font_face) {
  if (!font_face) {
    return std::nullopt;
  }

  static constexpr FontTableTag kOS2Tag = SetFourByteTag('O', 'S', '/', '2');
  static constexpr size_t kWeightOffset = 4;
  static constexpr size_t kWidthOffset = 6;
  static constexpr size_t kFsSelectionOffset = 62;
  static constexpr size_t kRequiredSize = kFsSelectionOffset + 2;
  static constexpr uint16_t kItalicSelectionBit = 1u << 0;
  static constexpr uint16_t kObliqueSelectionBit = 1u << 9;

  AutoDWriteTable os2_table(font_face, EndianSwap32(kOS2Tag));
  if (!os2_table.exists || os2_table.size < kRequiredSize) {
    return std::nullopt;
  }

  uint16_t os2_weight = ReadBigEndianUInt16(os2_table.data + kWeightOffset);
  uint16_t os2_width = ReadBigEndianUInt16(os2_table.data + kWidthOffset);
  if (os2_weight == 0 || os2_weight > 1000 || os2_width == 0 || os2_width > 9) {
    return std::nullopt;
  }

  uint16_t fs_selection =
      ReadBigEndianUInt16(os2_table.data + kFsSelectionOffset);
  FontStyle::Slant slant = FontStyle::kUpright_Slant;
  if (fs_selection & kItalicSelectionBit) {
    slant = FontStyle::kItalic_Slant;
  } else if (fs_selection & kObliqueSelectionBit) {
    slant = FontStyle::kOblique_Slant;
  }

  DWRITE_FONT_SIMULATIONS simulations = font_face->GetSimulations();
  if (simulations & DWRITE_FONT_SIMULATIONS_BOLD) {
    os2_weight = FontStyle::kBold_Weight;
  }
  if (simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
    slant = FontStyle::kOblique_Slant;
  }

  return FontStyle(static_cast<int>(os2_weight), static_cast<int>(os2_width),
                   slant);
}

std::string GetDWriteFamilyName(IDWriteFontFace3* font_face3,
                                IDWriteFontFamily* font_family,
                                const std::wstring& locale_name) {
  ScopedComPtr<IDWriteLocalizedStrings> family_names;
  if (font_face3 && SUCCEEDED(font_face3->GetFamilyNames(&family_names))) {
    return CopyLocalizedString(family_names.get(), locale_name);
  }

  if (font_family && SUCCEEDED(font_family->GetFamilyNames(&family_names))) {
    return CopyLocalizedString(family_names.get(), locale_name);
  }

  return "";
}

std::string GetDWritePostScriptName(IDWriteFontFace3* font_face3,
                                    const std::wstring& locale_name) {
  ScopedComPtr<IDWriteLocalizedStrings> post_script_names;
  BOOL exists = FALSE;
  if (!font_face3 ||
      FAILED(font_face3->GetInformationalStrings(
          DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &post_script_names,
          &exists)) ||
      !exists) {
    return "";
  }

  return CopyLocalizedString(post_script_names.get(), locale_name);
}

std::string GetDWritePostScriptName(IDWriteFont* font,
                                    const std::wstring& locale_name) {
  ScopedComPtr<IDWriteLocalizedStrings> post_script_names;
  BOOL exists = FALSE;
  if (!font ||
      FAILED(font->GetInformationalStrings(
          DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME, &post_script_names,
          &exists)) ||
      !exists) {
    return "";
  }

  return CopyLocalizedString(post_script_names.get(), locale_name);
}

std::string DecodeNameTableString(const uint8_t* data, size_t size,
                                  uint16_t platform_id) {
  std::string result;
  if (!data || size == 0) {
    return result;
  }

  if (platform_id == 3) {
    if ((size % 2) != 0) {
      return result;
    }
    result.reserve(size / 2);
    for (size_t i = 0; i < size; i += 2) {
      uint16_t value = ReadBigEndianUInt16(data + i);
      if (value == 0 || value > 0x7F) {
        return "";
      }
      result.push_back(static_cast<char>(value));
    }
    return result;
  }

  result.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    if (data[i] == 0 || data[i] > 0x7F) {
      return "";
    }
    result.push_back(static_cast<char>(data[i]));
  }
  return result;
}

int NameRecordPriority(uint16_t platform_id, uint16_t encoding_id,
                       uint16_t language_id) {
  if (platform_id == 3 && (encoding_id == 1 || encoding_id == 10) &&
      language_id == 0x0409) {
    return 0;
  }
  if (platform_id == 3 && (encoding_id == 1 || encoding_id == 10)) {
    return 1;
  }
  if (platform_id == 1 && encoding_id == 0) {
    return 2;
  }
  return 3;
}

std::string GetNameTablePostScriptName(IDWriteFontFace* font_face) {
  if (!font_face) {
    return "";
  }

  static constexpr FontTableTag kNameTag = SetFourByteTag('n', 'a', 'm', 'e');
  static constexpr uint16_t kPostScriptNameId = 6;
  static constexpr size_t kHeaderSize = 6;
  static constexpr size_t kRecordSize = 12;

  AutoDWriteTable name_table(font_face, EndianSwap32(kNameTag));
  if (!name_table.exists || name_table.size < kHeaderSize) {
    return "";
  }

  const uint8_t* data = name_table.data;
  const size_t table_size = static_cast<size_t>(name_table.size);
  uint16_t count = ReadBigEndianUInt16(data + 2);
  uint16_t string_offset = ReadBigEndianUInt16(data + 4);
  const size_t record_table_size = static_cast<size_t>(count) * kRecordSize;
  if (!RangeInBounds(kHeaderSize, record_table_size, table_size) ||
      string_offset >= table_size) {
    return "";
  }

  std::string best_name;
  int best_priority = 4;
  for (uint16_t i = 0; i < count; ++i) {
    const size_t record_offset =
        kHeaderSize + static_cast<size_t>(i) * kRecordSize;
    const uint8_t* record = data + record_offset;
    uint16_t platform_id = ReadBigEndianUInt16(record);
    uint16_t encoding_id = ReadBigEndianUInt16(record + 2);
    uint16_t language_id = ReadBigEndianUInt16(record + 4);
    uint16_t name_id = ReadBigEndianUInt16(record + 6);
    uint16_t length = ReadBigEndianUInt16(record + 8);
    uint16_t offset = ReadBigEndianUInt16(record + 10);
    if (name_id != kPostScriptNameId) {
      continue;
    }

    const size_t name_offset =
        static_cast<size_t>(string_offset) + static_cast<size_t>(offset);
    if (!RangeInBounds(name_offset, length, table_size)) {
      continue;
    }

    std::string name =
        DecodeNameTableString(data + name_offset, length, platform_id);
    if (name.empty()) {
      continue;
    }

    int priority = NameRecordPriority(platform_id, encoding_id, language_id);
    if (priority < best_priority) {
      best_name = std::move(name);
      best_priority = priority;
    }
  }

  return best_name;
}

std::shared_ptr<Data> CopyDWriteFontFileStreamData(
    IDWriteFontFileStream* stream) {
  if (!stream) {
    return nullptr;
  }

  UINT64 file_size = 0;
  if (FAILED(stream->GetFileSize(&file_size)) || file_size == 0 ||
      file_size > static_cast<UINT64>(std::numeric_limits<size_t>::max())) {
    return nullptr;
  }

  const void* fragment_start = nullptr;
  void* fragment_context = nullptr;
  if (FAILED(stream->ReadFileFragment(&fragment_start, 0, file_size,
                                      &fragment_context)) ||
      !fragment_start) {
    return nullptr;
  }

  auto data =
      Data::MakeWithCopy(fragment_start, static_cast<size_t>(file_size));
  stream->ReleaseFileFragment(fragment_context);
  return data && !data->IsEmpty() ? data : nullptr;
}

std::shared_ptr<Data> CopyDWriteFontFileData(IDWriteFontFile* font_file) {
  if (!font_file) {
    return nullptr;
  }

  const void* key = nullptr;
  UINT32 key_size = 0;
  font_file->GetReferenceKey(&key, &key_size);

  ScopedComPtr<IDWriteFontFileLoader> loader;
  if (FAILED(font_file->GetLoader(&loader))) {
    return nullptr;
  }

  ScopedComPtr<IDWriteFontFileStream> stream;
  if (FAILED(loader->CreateStreamFromKey(key, key_size, &stream))) {
    return nullptr;
  }

  return CopyDWriteFontFileStreamData(stream.get());
}

std::shared_ptr<Data> CopyDWriteFontFaceData(IDWriteFontFace* font_face) {
  std::vector<ScopedComPtr<IDWriteFontFile>> files;
  if (!font_face || !GetDWriteFontFiles(font_face, files) ||
      files.size() != 1) {
    return nullptr;
  }

  return CopyDWriteFontFileData(files[0].get());
}

FontStyle FontStyleFromDWriteFontFace(IDWriteFontFace* font_face) {
  if (!font_face) {
    return FontStyle::Normal();
  }

  auto table_style = FontStyleFromOS2Table(font_face);
  if (table_style) {
    return ApplyVariationStyle(font_face, *table_style);
  }

  int weight = FontStyle::kNormal_Weight;
  FontStyle::Slant slant = FontStyle::kUpright_Slant;
  DWRITE_FONT_SIMULATIONS simulations = font_face->GetSimulations();
  if (simulations & DWRITE_FONT_SIMULATIONS_BOLD) {
    weight = FontStyle::kBold_Weight;
  }
  if (simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
    slant = FontStyle::kOblique_Slant;
  }

  return ApplyVariationStyle(
      font_face, FontStyle(weight, FontStyle::kNormal_Width, slant));
}

FontStyle FontStyleFromDWriteTypeface(IDWriteFont* font,
                                      IDWriteFontFace* font_face) {
  auto table_style = FontStyleFromOS2Table(font_face);
  if (table_style) {
    return ApplyVariationStyle(font_face, *table_style);
  }
  FontStyle style = font ? FontStyleFromDWriteFont(font)
                         : FontStyleFromDWriteFontFace(font_face);
  return ApplyVariationStyle(font_face, style);
}

class TypefaceDWrite : public Typeface {
 public:
  TypefaceDWrite(
      IDWriteFactory* factory, IDWriteFontFace* font_face, IDWriteFont* font,
      IDWriteFontFamily* font_family, std::wstring locale_name,
      bool prefer_directwrite_post_script_name = false,
      std::shared_ptr<RegisteredFontFileLoader> registered_loader = nullptr,
      std::shared_ptr<RegisteredFontCollectionLoader>
          registered_collection_loader = nullptr,
      std::string directwrite_post_script_name_override = {},
      std::shared_ptr<Data> source_data = nullptr)
      : Typeface(FontStyleFromDWriteTypeface(font, font_face)),
        factory_(RefComPtr(factory)),
        registered_loader_(std::move(registered_loader)),
        registered_collection_loader_(std::move(registered_collection_loader)),
        font_face_(RefComPtr(font_face)),
        font_(SafeRefComPtr(font)),
        font_family_(SafeRefComPtr(font_family)),
        locale_name_(std::move(locale_name)),
        prefer_directwrite_post_script_name_(
            prefer_directwrite_post_script_name),
        directwrite_post_script_name_override_(
            std::move(directwrite_post_script_name_override)),
        source_data_(std::move(source_data)) {
    (void)font_face_->QueryInterface(&font_face2_);
    (void)font_face_->QueryInterface(&font_face3_);
#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
    (void)font_face_->QueryInterface(&font_face5_);
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__
  }

 protected:
  int OnGetTableTags(FontTableTag tags[]) const override {
    auto data =
        source_data_ ? source_data_ : CopyDWriteFontFaceData(font_face_.get());
    return GetSfntTableTags(data, font_face_->GetIndex(), tags);
  }

  size_t OnGetTableData(FontTableTag tag, size_t offset, size_t length,
                        void* data) const override {
    AutoDWriteTable table(font_face_.get(), EndianSwap32(tag));
    if (!table.exists) {
      return 0;
    }

    size_t table_size = static_cast<size_t>(table.size);
    if (offset > table_size) {
      return 0;
    }

    size_t copy_size = std::min(length, table_size - offset);
    if (data) {
      std::memcpy(data, table.data + offset, copy_size);
    }

    return copy_size;
  }

  void OnCharsToGlyphs(const uint32_t* chars, int count,
                       GlyphID glyphs[]) const override {
    if (!chars || !glyphs || count <= 0) {
      return;
    }

    ScopedComPtr<IDWriteTextAnalyzer> analyzer;
    if (FAILED(factory_->CreateTextAnalyzer(&analyzer))) {
      std::memset(glyphs, 0, static_cast<size_t>(count) * sizeof(GlyphID));
      return;
    }

    for (int i = 0; i < count; i++) {
      glyphs[i] = MapUnicharToGlyph(analyzer.get(), chars[i]);
    }
  }

  std::shared_ptr<Data> OnGetData() override {
    if (!source_data_) {
      source_data_ = CopyDWriteFontFaceData(font_face_.get());
    }
    return source_data_;
  }

  uint32_t OnGetUPEM() const override {
    DWRITE_FONT_METRICS metrics;
    font_face_->GetMetrics(&metrics);
    return metrics.designUnitsPerEm;
  }

  bool OnContainsColorTable() const override {
    if (font_face2_) {
      return font_face2_->IsColorFont();
    }

    static constexpr FontTableTag kColrTag = SetFourByteTag('C', 'O', 'L', 'R');
    static constexpr FontTableTag kCpalTag = SetFourByteTag('C', 'P', 'A', 'L');
    static constexpr FontTableTag kCbdtTag = SetFourByteTag('C', 'B', 'D', 'T');
    static constexpr FontTableTag kSbixTag = SetFourByteTag('s', 'b', 'i', 'x');
    AutoDWriteTable colr_table(font_face_.get(), EndianSwap32(kColrTag));
    AutoDWriteTable cpal_table(font_face_.get(), EndianSwap32(kCpalTag));
    AutoDWriteTable cbdt_table(font_face_.get(), EndianSwap32(kCbdtTag));
    AutoDWriteTable sbix_table(font_face_.get(), EndianSwap32(kSbixTag));
    return (colr_table.exists && cpal_table.exists) || cbdt_table.exists ||
           sbix_table.exists;
  }

  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc* desc) const override {
    return MakeScalerContextDWrite(
        const_cast<TypefaceDWrite*>(this)->shared_from_this(), factory_.get(),
        font_face_.get(), desc);
  }

  VariationPosition OnGetVariationDesignPosition() const override {
#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
    if (font_face5_ && font_face5_->HasVariations()) {
      return GetDWriteVariationDesignPosition(font_face5_.get());
    }
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__
    return VariationPosition{};
  }

  std::vector<VariationAxis> OnGetVariationDesignParameters() const override {
#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
    if (font_face5_ && font_face5_->HasVariations()) {
      return GetDWriteVariationDesignParameters(font_face5_.get());
    }
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__
    return {};
  }

  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments& args) const override {
    if (font_face_->GetIndex() != args.GetCollectionIndex()) {
      if (args.GetCollectionIndex() >
          static_cast<size_t>(std::numeric_limits<int>::max())) {
        return nullptr;
      }

      auto data = source_data_ ? source_data_
                               : CopyDWriteFontFaceData(font_face_.get());
      auto typeface = MakeDWriteTypefaceFromData(
          factory_.get(), locale_name_, std::move(data),
          static_cast<int>(args.GetCollectionIndex()));
      return typeface ? typeface->MakeVariation(args) : nullptr;
    }

#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
    if (font_face5_ && font_face5_->HasVariations()) {
      auto new_font_face = MakeDWriteVariationFontFace(font_face5_.get(), args);
      if (new_font_face) {
        std::string post_script_name_override =
            directwrite_post_script_name_override_;
        if (post_script_name_override.empty() &&
            prefer_directwrite_post_script_name_) {
          post_script_name_override =
              GetDWritePostScriptName(font_face3_.get(), locale_name_);
        }
        return std::make_shared<TypefaceDWrite>(
            factory_.get(), new_font_face.get(), font_.get(),
            font_family_.get(), locale_name_,
            prefer_directwrite_post_script_name_, registered_loader_,
            registered_collection_loader_, std::move(post_script_name_override),
            source_data_);
      }
    }
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__
    return const_cast<TypefaceDWrite*>(this)->shared_from_this();
  }

  void OnGetFontDescriptor(FontDescriptor& desc) const override {
    desc.style = GetFontStyle();

    std::string family_name = GetDWriteFamilyName(
        font_face3_.get(), font_family_.get(), locale_name_);
    if (!family_name.empty()) {
      desc.family_name = std::move(family_name);
    }

    desc.full_name.clear();
    std::string post_script_name = directwrite_post_script_name_override_;
    if (post_script_name.empty()) {
      if (prefer_directwrite_post_script_name_) {
        post_script_name =
            GetDWritePostScriptName(font_face3_.get(), locale_name_);
        if (post_script_name.empty()) {
          post_script_name = GetDWritePostScriptName(font_.get(), locale_name_);
        }
        if (post_script_name.empty()) {
          post_script_name = GetNameTablePostScriptName(font_face_.get());
        }
      } else {
        post_script_name = GetNameTablePostScriptName(font_face_.get());
        if (post_script_name.empty()) {
          post_script_name = GetDWritePostScriptName(font_.get(), locale_name_);
        }
        if (post_script_name.empty()) {
          post_script_name =
              GetDWritePostScriptName(font_face3_.get(), locale_name_);
        }
      }
    }
    if (!post_script_name.empty()) {
      desc.post_script_name = std::move(post_script_name);
    }

    desc.collection_index = static_cast<int32_t>(font_face_->GetIndex());
    desc.factory_id = kDWriteFactoryID;
  }

 private:
  GlyphID MapUnicharToGlyph(IDWriteTextAnalyzer* analyzer,
                            uint32_t codepoint) const {
    if (!analyzer || codepoint > 0x10FFFF ||
        (codepoint >= 0xD800 && codepoint <= 0xDFFF)) {
      return 0;
    }

    WCHAR text[2] = {};
    UINT32 text_length = 0;
    if (codepoint <= 0xFFFF) {
      text[0] = static_cast<WCHAR>(codepoint);
      text_length = 1;
    } else {
      codepoint -= 0x10000;
      text[0] = static_cast<WCHAR>(0xD800 + (codepoint >> 10));
      text[1] = static_cast<WCHAR>(0xDC00 + (codepoint & 0x3FF));
      text_length = 2;
    }

    DWRITE_SCRIPT_ANALYSIS script_analysis = {};
    UINT16 cluster_map[2] = {};
    DWRITE_SHAPING_TEXT_PROPERTIES text_props[2] = {};
    UINT16 glyph_indices[8] = {};
    DWRITE_SHAPING_GLYPH_PROPERTIES glyph_props[8] = {};
    UINT32 actual_glyph_count = 0;
    HRESULT hr = analyzer->GetGlyphs(
        text, text_length, font_face_.get(), FALSE, FALSE, &script_analysis,
        locale_name_.empty() ? nullptr : locale_name_.c_str(), nullptr, nullptr,
        nullptr, 0,
        static_cast<UINT32>(sizeof(glyph_indices) / sizeof(glyph_indices[0])),
        cluster_map, text_props, glyph_indices, glyph_props,
        &actual_glyph_count);
    if (FAILED(hr) || actual_glyph_count == 0) {
      return 0;
    }

    return glyph_indices[0];
  }

  ScopedComPtr<IDWriteFactory> factory_;
  std::shared_ptr<RegisteredFontFileLoader> registered_loader_;
  std::shared_ptr<RegisteredFontCollectionLoader> registered_collection_loader_;
  ScopedComPtr<IDWriteFontFace> font_face_;
  ScopedComPtr<IDWriteFontFace2> font_face2_;
  ScopedComPtr<IDWriteFontFace3> font_face3_;
#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
  ScopedComPtr<IDWriteFontFace5> font_face5_;
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__
  ScopedComPtr<IDWriteFont> font_;
  ScopedComPtr<IDWriteFontFamily> font_family_;
  std::wstring locale_name_;
  bool prefer_directwrite_post_script_name_;
  std::string directwrite_post_script_name_override_;
  std::shared_ptr<Data> source_data_;
};

std::shared_ptr<Typeface> MakeDWriteTypefaceFromCollection(
    IDWriteFactory* factory, const std::wstring& locale_name,
    IDWriteFontCollection* font_collection, int ttc_index,
    std::shared_ptr<RegisteredFontFileLoader> registered_loader,
    std::shared_ptr<RegisteredFontCollectionLoader>
        registered_collection_loader,
    std::shared_ptr<Data> source_data) {
  if (!factory || !font_collection || ttc_index < 0) {
    return nullptr;
  }

  UINT32 family_count = font_collection->GetFontFamilyCount();
  for (UINT32 family_index = 0; family_index < family_count; ++family_index) {
    ScopedComPtr<IDWriteFontFamily> font_family;
    HRNM(font_collection->GetFontFamily(family_index, &font_family),
         "Could not get DirectWrite custom font family.");

    UINT32 font_count = font_family->GetFontCount();
    for (UINT32 font_index = 0; font_index < font_count; ++font_index) {
      ScopedComPtr<IDWriteFont> font;
      HRNM(font_family->GetFont(font_index, &font),
           "Could not get DirectWrite custom font.");

      ScopedComPtr<IDWriteFontFace> font_face;
      HRNM(font->CreateFontFace(&font_face),
           "Could not create DirectWrite custom font face.");
      if (font_face->GetIndex() != static_cast<UINT32>(ttc_index)) {
        continue;
      }

#ifdef __IDWriteFontFace5_INTERFACE_DEFINED__
      auto default_font_face =
          MakeDWriteDefaultVariationFontFace(font_face.get());
      if (default_font_face) {
        font_face = std::move(default_font_face);
      }
#endif  // __IDWriteFontFace5_INTERFACE_DEFINED__

      return std::make_shared<TypefaceDWrite>(
          factory, font_face.get(), font.get(), font_family.get(), locale_name,
          true, std::move(registered_loader),
          std::move(registered_collection_loader), std::string{},
          std::move(source_data));
    }
  }

  return nullptr;
}

}  // namespace

std::string CopyLocalizedString(IDWriteLocalizedStrings* strings,
                                const std::wstring& locale_name) {
  if (strings == nullptr || strings->GetCount() == 0) {
    return "";
  }

  UINT32 index = 0;
  BOOL exists = FALSE;
  bool found = false;
  if (!locale_name.empty() &&
      SUCCEEDED(
          strings->FindLocaleName(locale_name.c_str(), &index, &exists)) &&
      exists) {
    found = true;
  }
  if (!found && SUCCEEDED(strings->FindLocaleName(L"en-us", &index, &exists)) &&
      exists) {
    found = true;
  }
  if (!found) {
    index = 0;
  }

  UINT32 length = 0;
  if (FAILED(strings->GetStringLength(index, &length))) {
    return "";
  }

  std::wstring value(length + 1, L'\0');
  if (FAILED(strings->GetString(index, &value[0],
                                static_cast<UINT32>(value.size())))) {
    return "";
  }
  value.resize(length);

  std::string result;
  if (FAILED(StrConversion::WideStringToString(value, &result))) {
    return "";
  }
  return result;
}

std::shared_ptr<Typeface> MakeDWriteTypeface(
    IDWriteFactory* factory, IDWriteFontFace* font_face, IDWriteFont* font,
    IDWriteFontFamily* font_family, std::wstring locale_name,
    bool prefer_directwrite_post_script_name,
    std::string directwrite_post_script_name_override,
    std::shared_ptr<Data> source_data) {
  return std::make_shared<TypefaceDWrite>(
      factory, font_face, font, font_family, std::move(locale_name),
      prefer_directwrite_post_script_name, nullptr, nullptr,
      std::move(directwrite_post_script_name_override), std::move(source_data));
}

std::shared_ptr<Typeface> MakeDWriteTypefaceFromDWriteFont(
    IDWriteFactory* factory, const std::wstring& locale_name,
    IDWriteFontFace* font_face, IDWriteFont* font,
    IDWriteFontFamily* font_family) {
  return MakeDWriteTypeface(factory, font_face, font, font_family, locale_name,
                            false, std::string{},
                            CopyDWriteFontFaceData(font_face));
}

std::shared_ptr<Typeface> MakeDWriteTypefaceFromFileReference(
    IDWriteFactory* factory, const std::wstring& locale_name, const char* path,
    int ttc_index) {
  if (!factory || !path || ttc_index < 0) {
    return nullptr;
  }

  std::wstring w_path;
  if (FAILED(StrConversion::StringToWideString(path, &w_path))) {
    return nullptr;
  }

  ScopedComPtr<IDWriteFontFile> font_file;
  if (FAILED(factory->CreateFontFileReference(w_path.c_str(), nullptr,
                                              &font_file))) {
    return nullptr;
  }

  BOOL is_supported = FALSE;
  DWRITE_FONT_FILE_TYPE file_type = DWRITE_FONT_FILE_TYPE_UNKNOWN;
  DWRITE_FONT_FACE_TYPE face_type = DWRITE_FONT_FACE_TYPE_UNKNOWN;
  UINT32 face_count = 0;
  if (FAILED(font_file->Analyze(&is_supported, &file_type, &face_type,
                                &face_count)) ||
      !is_supported || static_cast<UINT32>(ttc_index) >= face_count) {
    return nullptr;
  }

  IDWriteFontFile* font_files[] = {font_file.get()};
  ScopedComPtr<IDWriteFontFace> font_face;
  if (FAILED(factory->CreateFontFace(
          face_type, 1, font_files, static_cast<UINT32>(ttc_index),
          DWRITE_FONT_SIMULATIONS_NONE, &font_face))) {
    return nullptr;
  }

  auto source_data = Data::MakeFromFileMapping(path);
  return std::make_shared<TypefaceDWrite>(
      factory, font_face.get(), nullptr, nullptr, locale_name, true, nullptr,
      nullptr, std::string{}, std::move(source_data));
}

std::shared_ptr<Typeface> MakeDWriteTypefaceFromData(
    IDWriteFactory* factory, const std::wstring& locale_name,
    std::shared_ptr<Data> data, int ttc_index) {
  if (!factory || !data || data->IsEmpty() || ttc_index < 0) {
    return nullptr;
  }

  ScopedComPtr<DataFontFileLoader> loader(new DataFontFileLoader(data));
  auto registered_loader =
      RegisteredFontFileLoader::Make(factory, loader.get());
  if (!registered_loader) {
    return nullptr;
  }

  ScopedComPtr<DataFontCollectionLoader> collection_loader(
      new DataFontCollectionLoader(loader.get()));
  auto registered_collection_loader =
      RegisteredFontCollectionLoader::Make(factory, collection_loader.get());
  if (!registered_collection_loader) {
    return nullptr;
  }

  ScopedComPtr<IDWriteFontCollection> font_collection;
  if (FAILED(factory->CreateCustomFontCollection(
          collection_loader.get(), nullptr, 0, &font_collection))) {
    return nullptr;
  }

  return MakeDWriteTypefaceFromCollection(
      factory, locale_name, font_collection.get(), ttc_index,
      std::move(registered_loader), std::move(registered_collection_loader),
      std::move(data));
}

}  // namespace skity
