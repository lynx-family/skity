// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "wgsl/token.h"

namespace wgx {

struct Cursor {
  uint32_t line = 0;
  uint32_t column = 0;
};

class Scanner {
 public:
  explicit Scanner(std::string_view content);

  ~Scanner() = default;

  std::vector<Token> Scan();

 private:
  Token Next();

  bool IsEOF() const { return mPos >= mContent.size(); }

  uint8_t PeekChar(size_t offset = 0) const;

  /// https://www.w3.org/TR/WGSL/#blankspace-and-line-breaks
  bool IsBlankSpace() const;
  bool IsLineBreak() const;
  bool IsDigit(uint8_t c) const;
  bool IsHex(uint8_t c) const;
  bool IsAlphabet(uint8_t c) const;

  void Advance(size_t step = 1);
  void AdvanceLine();

  Token SkipSpaceAndComments();

  Token Number();

  Token HexNumber();

  std::optional<Token> Identity();

  std::optional<Token> Punctuation();

  bool Match(std::string_view str) const;

  /// Return a Token represent a comment or error
  Token Comments();

 private:
  std::string_view mContent;
  Cursor mCursor;
  size_t mPos = 0;
  bool mError = false;
};

}  // namespace wgx
