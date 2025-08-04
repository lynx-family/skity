// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_TEXT_BLOB_HPP
#define INCLUDE_SKITY_TEXT_TEXT_BLOB_HPP

#include <skity/graphic/paint.hpp>
#include <skity/macros.hpp>
#include <skity/text/text_run.hpp>
#include <string>
#include <vector>

namespace skity {

class Font;

/**
 * Immutable container which to hold TextRun.
 *
 */
class SKITY_API TextBlob final {
 public:
  TextBlob(std::vector<TextRun> runs) : text_run_(std::move(runs)) {}
  ~TextBlob() = default;
  TextBlob(const TextBlob&) = delete;
  TextBlob& operator=(const TextBlob&) = delete;

  std::vector<TextRun> const& GetTextRun() const { return text_run_; }

  Vec2 GetBoundSize() const;

  static Rect ComputeBounds(uint32_t count, const GlyphID* glyphs,
                            const float* position_x, const float* position_y,
                            const Font& font, const Paint& paint);

 private:
  std::vector<TextRun> text_run_ = {};
};

class SKITY_API TypefaceDelegate {
 public:
  virtual ~TypefaceDelegate() = default;
  TypefaceDelegate() = default;
  TypefaceDelegate(const TypefaceDelegate&) = delete;
  TypefaceDelegate& operator=(const TypefaceDelegate&) = delete;

  virtual std::shared_ptr<Typeface> Fallback(Unichar code_point,
                                             Paint const& text_paint) = 0;

  virtual std::vector<std::vector<Unichar>> BreakTextRun(const char* text) = 0;

  static std::unique_ptr<TypefaceDelegate> CreateSimpleFallbackDelegate(
      const std::vector<std::shared_ptr<Typeface>>& typefaces);
};

class SKITY_API TextBlobBuilder final {
 public:
  TextBlobBuilder() = default;
  ~TextBlobBuilder() = default;
  TextBlobBuilder& operator=(const TextBlobBuilder&) = delete;

  std::shared_ptr<TextBlob> BuildTextBlob(const char* text, Paint const& paint,
                                          TypefaceDelegate* delegate = nullptr);

  std::shared_ptr<TextBlob> BuildTextBlob(std::string const& text,
                                          Paint const& paint);

 private:
  std::shared_ptr<TextBlob> GenerateBlobWithoutDelegate(const char* text,
                                                        Paint const& paint);

  std::shared_ptr<TextBlob> GenerateBlobWithDelegate(
      const char* text, Paint const& paint, TypefaceDelegate* delegate);

  std::shared_ptr<TextBlob> GenerateBlobWithMultiRun(
      std::vector<std::vector<Unichar>> const& code_points, Paint const& paint,
      TypefaceDelegate* delegate);

  std::vector<TextRun> GenerateTextRuns(std::vector<Unichar> const& code_points,
                                        std::shared_ptr<Typeface> typeface,
                                        Paint const& paint,
                                        TypefaceDelegate* delegate);

  TextRun GenerateTextRun(std::vector<Unichar> const& code_points,
                          std::shared_ptr<Typeface> typeface, float font_size,
                          bool need_path);
};
}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_TEXT_BLOB_HPP
