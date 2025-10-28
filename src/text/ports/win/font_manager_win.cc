/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

// clang-format off
#include "src/text/ports/win/dwrite_version.hpp"
#include "src/base/platform/win/handle_result.hpp"
#include "src/base/platform/win/str_conversion.hpp"
#include "src/text/ports/win/scoped_com_ptr.hpp"
#include "src/text/ports/win/dwrite_utils.hpp"
// clang-format on

#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>
#include <winsdkver.h>

#include <cassert>
#include <mutex>
#include <skity/io/data.hpp>
#include <skity/text/font_arguments.hpp>
#include <skity/text/font_manager.hpp>
#include <skity/text/utf.hpp>

#include "src/logging.hpp"
#include "src/text/ports/typeface_freetype.hpp"
#include "src/utils/no_destructor.hpp"

namespace skity {

namespace {
static IDWriteFactory* gDWriteFactory = nullptr;

static void release_dwrite_factory() {
  if (gDWriteFactory) {
    gDWriteFactory->Release();
  }
}

static void create_dwrite_factory(IDWriteFactory** factory) {
  using DWriteCreateFactoryProc = decltype(DWriteCreateFactory)*;
  DWriteCreateFactoryProc dWriteCreateFactoryProc;

  HRESULT hr =
      LoadWinProc(&dWriteCreateFactoryProc, L"DWriteCore.dll",
                  LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, "DWriteCoreCreateFactory");
  if (!dWriteCreateFactoryProc) {
    hr = LoadWinProc(&dWriteCreateFactoryProc, L"dwrite.dll",
                     LOAD_LIBRARY_SEARCH_DEFAULT_DIRS, "DWriteCreateFactory");
  }

  if (!dWriteCreateFactoryProc) {
    HRVM(hr, "Could not get DWriteCreateFactory proc.");
  }

  HRVM(dWriteCreateFactoryProc(DWRITE_FACTORY_TYPE_SHARED,
                               __uuidof(IDWriteFactory),
                               reinterpret_cast<IUnknown**>(factory)),
       "Could not create DirectWrite factory.");
  atexit(release_dwrite_factory);
}

IDWriteFactory* get_dwrite_factory() {
  static std::once_flag flag;
  static IDWriteFactory* gDWriteFactory = nullptr;

  std::call_once(flag, [] { create_dwrite_factory(&gDWriteFactory); });

  return gDWriteFactory;
}

/** Reverse all 4 bytes in a 32bit value.
  e.g. 0x12345678 -> 0x78563412
*/
static constexpr uint32_t EndianSwap32(uint32_t value) {
  return ((value & 0xFF) << 24) | ((value & 0xFF00) << 8) |
         ((value & 0xFF0000) >> 8) | (value >> 24);
}

// Korean fonts Gulim, Dotum, Batang, Gungsuh have bitmap strikes that get
// artifically emboldened by Windows without antialiasing. Korean users prefer
// these over the synthetic boldening performed by Skia. So let's make an
// exception for fonts with bitmap strikes and allow passing through Windows
// simulations for those, until Skia provides more control over simulations in
// font matching, see https://crbug.com/1258378
bool HasBitmapStrikes(const ScopedComPtr<IDWriteFont>& font) {
  ScopedComPtr<IDWriteFontFace> fontFace;
  HRB(font->CreateFontFace(&fontFace));

  AutoDWriteTable ebdtTable(fontFace.get(),
                            EndianSwap32(SetFourByteTag('E', 'B', 'D', 'T')));
  return ebdtTable.exists;
}

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

}  // namespace

class FontFallbackSource : public IDWriteTextAnalysisSource {
 public:
  FontFallbackSource(const WCHAR* string, UINT32 length, const WCHAR* locale,
                     IDWriteNumberSubstitution* numberSubstitution)
      : fRefCount(1),
        fString(string),
        fLength(length),
        fLocale(locale),
        fNumberSubstitution(numberSubstitution) {}

  // IUnknown methods
  SK_STDMETHODIMP QueryInterface(IID const& riid, void** ppvObject) override {
    if (__uuidof(IUnknown) == riid ||
        __uuidof(IDWriteTextAnalysisSource) == riid) {
      *ppvObject = this;
      this->AddRef();
      return S_OK;
    }
    *ppvObject = nullptr;
    return E_FAIL;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&fRefCount);
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG newCount = InterlockedDecrement(&fRefCount);
    if (0 == newCount) {
      delete this;
    }
    return newCount;
  }

  // IDWriteTextAnalysisSource methods
  SK_STDMETHODIMP GetTextAtPosition(UINT32 textPosition,
                                    WCHAR const** textString,
                                    UINT32* textLength) override {
    if (fLength <= textPosition) {
      *textString = nullptr;
      *textLength = 0;
      return S_OK;
    }
    *textString = fString + textPosition;
    *textLength = fLength - textPosition;
    return S_OK;
  }

  SK_STDMETHODIMP GetTextBeforePosition(UINT32 textPosition,
                                        WCHAR const** textString,
                                        UINT32* textLength) override {
    if (textPosition < 1 || fLength <= textPosition) {
      *textString = nullptr;
      *textLength = 0;
      return S_OK;
    }
    *textString = fString;
    *textLength = textPosition;
    return S_OK;
  }

  SK_STDMETHODIMP_(DWRITE_READING_DIRECTION)
  GetParagraphReadingDirection() override {
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
  }

  SK_STDMETHODIMP GetLocaleName(UINT32 textPosition, UINT32* textLength,
                                WCHAR const** localeName) override {
    *localeName = fLocale;
    return S_OK;
  }

  SK_STDMETHODIMP GetNumberSubstitution(
      UINT32 textPosition, UINT32* textLength,
      IDWriteNumberSubstitution** numberSubstitution) override {
    *numberSubstitution = fNumberSubstitution;
    return S_OK;
  }

 private:
  virtual ~FontFallbackSource() {}

  ULONG fRefCount;
  const WCHAR* fString;
  UINT32 fLength;
  const WCHAR* fLocale;
  IDWriteNumberSubstitution* fNumberSubstitution;
};

class FontManagerWin;
class FontStyleSetWin : public FontStyleSet {
 public:
  explicit FontStyleSetWin(const FontManagerWin* font_manager,
                           IDWriteFontFamily* font_family)
      : font_manager_(font_manager), font_family_(RefComPtr(font_family)) {}

  int Count() override { return font_family_->GetFontCount(); }

  void GetStyle(int index, FontStyle* style, std::string* name) override;

  std::shared_ptr<Typeface> CreateTypeface(int index) override;

  std::shared_ptr<Typeface> MatchStyle(const FontStyle& pattern) override;

 private:
  const FontManagerWin* font_manager_;
  ScopedComPtr<IDWriteFontFamily> font_family_;

  friend class FontManagerWin;
};

class FontManagerWin : public FontManager {
 public:
  FontManagerWin(IDWriteFactory* factory,
                 IDWriteFontCollection* font_collection,
                 IDWriteFontFallback* fallback, std::wstring locale_name)
      : factory_(RefComPtr(factory)),
        font_collection_(RefComPtr(font_collection)),
        fallback_(SafeRefComPtr(fallback)),
        locale_name_(std::move(locale_name)) {}

  std::shared_ptr<Typeface> MakeTypefaceFromDWriteFont(
      IDWriteFontFace* font_face, IDWriteFont* font,
      IDWriteFontFamily* font_family) const;

 protected:
  int OnCountFamilies() const override {
    return font_collection_->GetFontFamilyCount();
  }

  std::string OnGetFamilyName(int) const override {
    LOGE("onGetFamilyName called with bad index");
    return "";
  }

  std::shared_ptr<FontStyleSet> OnCreateStyleSet(int index) const override {
    ScopedComPtr<IDWriteFontFamily> font_family;
    HRNM(font_collection_->GetFontFamily(index, &font_family),
         "Could not get requested family.");
    return std::make_shared<FontStyleSetWin>(this, font_family.get());
  }

  std::shared_ptr<FontStyleSet> OnMatchFamily(
      const char family_name[]) const override;

  std::shared_ptr<Typeface> OnMatchFamilyStyle(
      const char family_name[], const FontStyle& style) const override;

  std::shared_ptr<Typeface> OnMatchFamilyStyleCharacter(const char[],
                                                        const FontStyle&,
                                                        const char*[], int,
                                                        Unichar) const override;

  std::shared_ptr<Typeface> OnMakeFromData(std::shared_ptr<Data> const& data,
                                           int ttcIndex) const override {
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  std::shared_ptr<Typeface> OnMakeFromFile(const char path[],
                                           int ttcIndex) const override {
    auto data = Data::MakeFromFileName(path);
    return TypefaceFreeType::Make(data,
                                  FontArguments().SetCollectionIndex(ttcIndex));
  }

  std::shared_ptr<Typeface> OnGetDefaultTypeface(
      FontStyle const&) const override {
    return nullptr;
  }

 private:
  ScopedComPtr<IDWriteFactory> factory_;
  ScopedComPtr<IDWriteFontCollection> font_collection_;
  ScopedComPtr<IDWriteFontFallback> fallback_;
  std::wstring locale_name_;

  std::shared_ptr<Typeface> fallback(const WCHAR* dwFamilyName, DWriteStyle,
                                     const WCHAR* dwBcp47,
                                     UINT32 character) const;

  std::shared_ptr<Typeface> layoutFallback(const WCHAR* dwFamilyName,
                                           DWriteStyle, const WCHAR* dwBcp47,
                                           UINT32 character) const;

  friend class FontFallbackRenderer;
};

class FontFallbackRenderer : public IDWriteTextRenderer {
 public:
  FontFallbackRenderer(const FontManagerWin* font_manager, UINT32 character)
      : ref_count_(1),
        font_manager_(font_manager),
        character_(character),
        fallback_typeface_(nullptr) {}

  // IUnknown methods
  SK_STDMETHODIMP QueryInterface(IID const& riid, void** ppvObject) override {
    if (__uuidof(IUnknown) == riid || __uuidof(IDWritePixelSnapping) == riid ||
        __uuidof(IDWriteTextRenderer) == riid) {
      *ppvObject = this;
      this->AddRef();
      return S_OK;
    }
    *ppvObject = nullptr;
    return E_FAIL;
  }

  SK_STDMETHODIMP_(ULONG) AddRef() override {
    return InterlockedIncrement(&ref_count_);
  }

  SK_STDMETHODIMP_(ULONG) Release() override {
    ULONG newCount = InterlockedDecrement(&ref_count_);
    if (0 == newCount) {
      delete this;
    }
    return newCount;
  }

  // IDWriteTextRenderer methods
  SK_STDMETHODIMP DrawGlyphRun(
      void* clientDrawingContext, FLOAT baselineOriginX, FLOAT baselineOriginY,
      DWRITE_MEASURING_MODE measuringMode, DWRITE_GLYPH_RUN const* glyphRun,
      DWRITE_GLYPH_RUN_DESCRIPTION const* glyphRunDescription,
      IUnknown* clientDrawingEffect) override {
    if (!glyphRun->fontFace) {
      HRM(E_INVALIDARG, "Glyph run without font face.");
    }

    ScopedComPtr<IDWriteFont> font;
    HRM(font_manager_->font_collection_->GetFontFromFontFace(glyphRun->fontFace,
                                                             &font),
        "Could not get font from font face.");

    // It is possible that the font passed does not actually have the requested
    // character, due to no font being found and getting the fallback font.
    // Check that the font actually contains the requested character.
    BOOL exists;
    HRM(font->HasCharacter(character_, &exists), "Could not find character.");

    if (exists) {
      ScopedComPtr<IDWriteFontFamily> fontFamily;
      HRM(font->GetFontFamily(&fontFamily), "Could not get family.");
      fallback_typeface_ = font_manager_->MakeTypefaceFromDWriteFont(
          glyphRun->fontFace, font.get(), fontFamily.get());
      has_simulations_ =
          (font->GetSimulations() != DWRITE_FONT_SIMULATIONS_NONE) &&
          !HasBitmapStrikes(font);
    }

    return S_OK;
  }

  SK_STDMETHODIMP DrawUnderline(void* clientDrawingContext,
                                FLOAT baselineOriginX, FLOAT baselineOriginY,
                                DWRITE_UNDERLINE const* underline,
                                IUnknown* clientDrawingEffect) override {
    return E_NOTIMPL;
  }

  SK_STDMETHODIMP DrawStrikethrough(void* clientDrawingContext,
                                    FLOAT baselineOriginX,
                                    FLOAT baselineOriginY,
                                    DWRITE_STRIKETHROUGH const* strikethrough,
                                    IUnknown* clientDrawingEffect) override {
    return E_NOTIMPL;
  }

  SK_STDMETHODIMP DrawInlineObject(void* clientDrawingContext, FLOAT originX,
                                   FLOAT originY,
                                   IDWriteInlineObject* inlineObject,
                                   BOOL isSideways, BOOL isRightToLeft,
                                   IUnknown* clientDrawingEffect) override {
    return E_NOTIMPL;
  }

  // IDWritePixelSnapping methods
  SK_STDMETHODIMP IsPixelSnappingDisabled(void* clientDrawingContext,
                                          BOOL* isDisabled) override {
    *isDisabled = FALSE;
    return S_OK;
  }

  SK_STDMETHODIMP GetCurrentTransform(void* clientDrawingContext,
                                      DWRITE_MATRIX* transform) override {
    const DWRITE_MATRIX ident = {1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    *transform = ident;
    return S_OK;
  }

  SK_STDMETHODIMP GetPixelsPerDip(void* clientDrawingContext,
                                  FLOAT* pixelsPerDip) override {
    *pixelsPerDip = 1.0f;
    return S_OK;
  }

  std::shared_ptr<Typeface> ConsumeFallbackTypeface() {
    return std::move(fallback_typeface_);
  }

  bool FallbackTypefaceHasSimulations() { return has_simulations_; }

 private:
  virtual ~FontFallbackRenderer() {}

  ULONG ref_count_;
  const FontManagerWin* font_manager_;
  UINT32 character_;
  std::shared_ptr<Typeface> fallback_typeface_;
  bool has_simulations_{false};
};

/// FontStyleSetWin Impl

void FontStyleSetWin::GetStyle(int index, FontStyle* style, std::string* name) {
  ScopedComPtr<IDWriteFont> font;
  HRVM(font_family_->GetFont(index, &font), "Could not get font.");

  // if (style) {
  //   ScopedComPtr<IDWriteFontFace> face;
  //   HRVM(font->CreateFontFace(&face), "Could not get face.");
  //   *fs = DWriteFontTypeface::GetStyle(font.get(), face.get());
  // }

  // if (name) {
  //   ScopedComPtr<IDWriteLocalizedStrings> faceNames;
  //   if (SUCCEEDED(font->GetFaceNames(&faceNames))) {
  //     sk_get_locale_string(faceNames.get(), fFontMgr->fLocaleName.get(),
  //                          styleName);
  //   }
  // }
}

std::shared_ptr<Typeface> FontStyleSetWin::CreateTypeface(int index) {
  ScopedComPtr<IDWriteFont> font;
  HRNM(font_family_->GetFont(index, &font), "Could not get font.");

  ScopedComPtr<IDWriteFontFace> fontFace;
  HRNM(font->CreateFontFace(&fontFace), "Could not create font face.");

  return font_manager_->MakeTypefaceFromDWriteFont(fontFace.get(), font.get(),
                                                   font_family_.get());
}

std::shared_ptr<Typeface> FontStyleSetWin::MatchStyle(
    const FontStyle& pattern) {
  ScopedComPtr<IDWriteFont> font;
  DWriteStyle dwStyle(pattern);

  HRNM(FirstMatchingFontWithoutSimulations(font_family_, dwStyle, font),
       "No font found from family.");

  ScopedComPtr<IDWriteFontFace> fontFace;
  HRNM(font->CreateFontFace(&fontFace), "Could not create font face.");

  return font_manager_->MakeTypefaceFromDWriteFont(fontFace.get(), font.get(),
                                                   font_family_.get());
}

/// FontManagerWin Impl

std::shared_ptr<FontStyleSet> FontManagerWin::OnMatchFamily(
    const char family_name[]) const {
  if (!family_name) {
    return nullptr;
  }

  std::wstring w_family_name;
  HRN(StrConversion::StringToWideString(family_name, &w_family_name));

  UINT32 index;
  BOOL exists;
  HRNM(font_collection_->FindFamilyName(w_family_name.c_str(), &index, &exists),
       "Failed while finding family by name.");
  if (!exists) {
    return nullptr;
  }

  return this->OnCreateStyleSet(index);
}

std::shared_ptr<Typeface> FontManagerWin::OnMatchFamilyStyle(
    const char family_name[], const FontStyle& style) const {
  std::shared_ptr<FontStyleSet> sset(this->MatchFamily(family_name));
  return sset->MatchStyle(style);
}

std::shared_ptr<Typeface> FontManagerWin::OnMatchFamilyStyleCharacter(
    const char family_name[], const FontStyle& style, const char* bcp47[],
    int bcp47_count, Unichar character) const {
  DWriteStyle dwStyle(style);

  std::wstring w_family_name;
  if (family_name) {
    HRN(StrConversion::StringToWideString(family_name, &w_family_name));
  }

  std::wstring dw_bcp47;
  if (bcp47_count < 1) {
    dw_bcp47 = locale_name_;
  } else {
    HRN(StrConversion::StringToWideString(bcp47[bcp47_count - 1], &dw_bcp47));
  }

  if (fallback_) {
    return this->fallback(w_family_name.c_str(), dwStyle, dw_bcp47.c_str(),
                          character);
  }

  // Windows 7 does not support font fallback and performs a single layout pass
  // to find a suitable font.
  return layoutFallback(w_family_name.c_str(), dwStyle, dw_bcp47.c_str(),
                        character);
}

std::shared_ptr<Typeface> FontManagerWin::fallback(const WCHAR* dwFamilyName,
                                                   DWriteStyle dwStyle,
                                                   const WCHAR* dwBcp47,
                                                   UINT32 character) const {
  WCHAR utf16[16];
  size_t utf16_len =
      UTF::ConvertToUTF16(character, reinterpret_cast<uint16_t*>(utf16));

  ScopedComPtr<IDWriteNumberSubstitution> numberSubstitution;
  HRNM(
      factory_->CreateNumberSubstitution(DWRITE_NUMBER_SUBSTITUTION_METHOD_NONE,
                                         dwBcp47, TRUE, &numberSubstitution),
      "Could not create number substitution.");
  ScopedComPtr<FontFallbackSource> fontFallbackSource(new FontFallbackSource(
      utf16, utf16_len, dwBcp47, numberSubstitution.get()));

  UINT32 mappedLength;
  ScopedComPtr<IDWriteFont> font;
  FLOAT scale;

  bool noSimulations = false;
  while (!noSimulations) {
    font.reset();
    HRNM(fallback_->MapCharacters(fontFallbackSource.get(),
                                  0,  // textPosition,
                                  utf16_len, font_collection_.get(),
                                  dwFamilyName, dwStyle.weight, dwStyle.slant,
                                  dwStyle.width, &mappedLength, &font, &scale),
         "Could not map characters");
    if (!font.get()) {
      return nullptr;
    }

    DWRITE_FONT_SIMULATIONS simulations = font->GetSimulations();
    noSimulations =
        simulations == DWRITE_FONT_SIMULATIONS_NONE || HasBitmapStrikes(font);

    if (simulations & DWRITE_FONT_SIMULATIONS_BOLD) {
      dwStyle.weight = DWRITE_FONT_WEIGHT_REGULAR;
      continue;
    }

    if (simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE) {
      dwStyle.slant = DWRITE_FONT_STYLE_NORMAL;
      continue;
    }
  }

  ScopedComPtr<IDWriteFontFace> fontFace;
  HRNM(font->CreateFontFace(&fontFace), "Could not get font face from font.");

  ScopedComPtr<IDWriteFontFamily> fontFamily;
  HRNM(font->GetFontFamily(&fontFamily), "Could not get family from font.");
  return this->MakeTypefaceFromDWriteFont(fontFace.get(), font.get(),
                                          fontFamily.get());
}

std::shared_ptr<Typeface> FontManagerWin::layoutFallback(
    const WCHAR* dwFamilyName, DWriteStyle dwStyle, const WCHAR* dwBcp47,
    UINT32 character) const {
  WCHAR utf16[16];
  size_t utf16_len =
      UTF::ConvertToUTF16(character, reinterpret_cast<uint16_t*>(utf16));

  bool noSimulations = false;
  std::shared_ptr<Typeface> fallback_typeface{nullptr};
  while (!noSimulations) {
    ScopedComPtr<IDWriteTextFormat> fallbackFormat;
    HRNM(factory_->CreateTextFormat(dwFamilyName ? dwFamilyName : L"",
                                    font_collection_.get(), dwStyle.weight,
                                    dwStyle.slant, dwStyle.width, 72.0f,
                                    dwBcp47, &fallbackFormat),
         "Could not create text format.");

    // No matter how the font collection is set on this IDWriteTextLayout, it is
    // not possible to
    // disable use of the system font collection in fallback.
    ScopedComPtr<IDWriteTextLayout> fallbackLayout;
    HRNM(factory_->CreateTextLayout(utf16, utf16_len, fallbackFormat.get(),
                                    200.0f, 200.0f, &fallbackLayout),
         "Could not create text layout.");

    ScopedComPtr<FontFallbackRenderer> fontFallbackRenderer(
        new FontFallbackRenderer(this, character));
    HRNM(fallbackLayout->SetFontCollection(
             font_collection_.get(), {0, static_cast<uint32_t>(utf16_len)}),
         "Could not set layout font collection.");
    HRNM(
        fallbackLayout->Draw(nullptr, fontFallbackRenderer.get(), 50.0f, 50.0f),
        "Could not draw layout with renderer.");

    noSimulations = !fontFallbackRenderer->FallbackTypefaceHasSimulations();
    if (noSimulations) {
      fallback_typeface = fontFallbackRenderer->ConsumeFallbackTypeface();
    }

    if (dwStyle.weight != DWRITE_FONT_WEIGHT_REGULAR) {
      dwStyle.weight = DWRITE_FONT_WEIGHT_REGULAR;
      continue;
    }

    if (dwStyle.slant != DWRITE_FONT_STYLE_NORMAL) {
      dwStyle.slant = DWRITE_FONT_STYLE_NORMAL;
      continue;
    }
  }
  return fallback_typeface;
}

static std::wstring GetFontFilePath(IDWriteFontFile* fontFile) {
  ScopedComPtr<IDWriteFontFileLoader> loader;
  if (FAILED(fontFile->GetLoader(&loader))) {
    return L"";
  }

  ScopedComPtr<IDWriteLocalFontFileLoader> localLoader;
  HRESULT hr = loader->QueryInterface(IID_PPV_ARGS(&localLoader));
  if (FAILED(hr)) {
    LOGE("{}\n", "loader->QueryInterface failed.");
    return L"";
  }

  const void* key = nullptr;
  UINT32 keySize = 0;
  fontFile->GetReferenceKey(&key, &keySize);

  UINT32 pathLen = 0;
  if (FAILED(localLoader->GetFilePathLengthFromKey(key, keySize, &pathLen))) {
    return L"";
  }

  std::wstring path(pathLen, L'\0');
  if (FAILED(localLoader->GetFilePathFromKey(key, keySize, &path[0],
                                             pathLen + 1))) {
    LOGE("{}\n", "localLoader->GetFilePathFromKey failed.");
    return L"";
  }

  return path;
}

std::shared_ptr<Typeface> FontManagerWin::MakeTypefaceFromDWriteFont(
    IDWriteFontFace* font_face, IDWriteFont* font,
    IDWriteFontFamily* font_family) const {
  uint32_t number_of_files = 0;
  HRNM(font_face->GetFiles(&number_of_files, nullptr),
       "Could not get number of files from font face.");
  if (number_of_files == 0) {
    LOGE("{}\n", "Get 0 files from font face.");
    return nullptr;
  }

  std::vector<ScopedComPtr<IDWriteFontFile>> files(number_of_files);
  HRNM(font_face->GetFiles(&number_of_files,
                           reinterpret_cast<IDWriteFontFile**>(files.data()));
       , "Could not get files from font face.");

  for (auto& file : files) {
    std::wstring w_path = GetFontFilePath(file.get());
    if (!w_path.empty()) {
      std::string path;
      HRNM(StrConversion::WideStringToString(w_path, &path),
           "WideStringToString failed");
      LOGE("Font file path: {}\n", path);
      std::shared_ptr<Data> data = Data::MakeFromFileMapping(path.c_str());
      UINT32 ttc_index = font_face->GetIndex();
      return TypefaceFreeType::Make(
          data, FontArguments().SetCollectionIndex(ttc_index));
    } else {
      LOGE("Font file path not available (maybe custom loader)");
    }
  }

  return nullptr;
}

static std::shared_ptr<FontManager> InitFontManagerWin() {
  IDWriteFactory* factory = nullptr;
  IDWriteFontCollection* collection = nullptr;
  IDWriteFontFallback* fallback = nullptr;

  factory = get_dwrite_factory();
  if (nullptr == factory) {
    return nullptr;
  }

  ScopedComPtr<IDWriteFontCollection> systemFontCollection;
  HRBM(factory->GetSystemFontCollection(&systemFontCollection, FALSE),
       "Could not get system font collection.");
  collection = systemFontCollection.get();

  // It is possible to have been provided a font fallback when factory2 is not
  // available.
  ScopedComPtr<IDWriteFontFallback> systemFontFallback;
  ScopedComPtr<IDWriteFactory2> factory2;
  if (!SUCCEEDED(factory->QueryInterface(&factory2))) {
    // IUnknown::QueryInterface states that if it fails, punk will be set to
    // nullptr.
    // http://blogs.msdn.com/b/oldnewthing/archive/2004/03/26/96777.aspx
    assert(nullptr == factory2.get());
  } else {
    HRNM(factory2->GetSystemFontFallback(&systemFontFallback),
         "Could not get system fallback.");
    fallback = systemFontFallback.get();
  }

  WCHAR localeNameStorage[LOCALE_NAME_MAX_LENGTH];
  const WCHAR* localeName = L"";
  size_t localeNameLen = 1;

  // Dynamically load GetUserDefaultLocaleName function, as it is not
  // available on XP.
  typedef int(WINAPI * GetUserDefaultLocaleNameProc)(LPWSTR, int);  // NOLINT
  GetUserDefaultLocaleNameProc getUserDefaultLocaleNameProc = nullptr;
  HRESULT hr =
      LoadWinProc(&getUserDefaultLocaleNameProc, L"Kernel32.dll",
                  LOAD_LIBRARY_SEARCH_SYSTEM32, "GetUserDefaultLocaleName");
  if (nullptr == getUserDefaultLocaleNameProc) {
    HANDLE_RESULT(hr, "Could not get GetUserDefaultLocaleName.");
  } else {
    int size =
        getUserDefaultLocaleNameProc(localeNameStorage, LOCALE_NAME_MAX_LENGTH);
    if (size) {
      localeName = localeNameStorage;
      localeNameLen = size;
    }
  }
  std::wstring locale_name{localeName, localeNameLen - 1};

  return std::make_shared<FontManagerWin>(factory, collection, fallback,
                                          std::move(locale_name));
}

std::shared_ptr<FontManager> FontManager::RefDefault() {
  static const NoDestructor<std::shared_ptr<FontManager>> font_manager(
      [] { return InitFontManagerWin(); }());
  return *font_manager;
}

}  // namespace skity
