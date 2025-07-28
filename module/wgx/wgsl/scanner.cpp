// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "wgsl/scanner.h"

#include <charconv>
#include <cmath>
#include <unordered_map>

#ifdef WGX_DEBUG_SCANNER
#include <iostream>
#endif

namespace wgx {

static const std::unordered_map<std::string_view, TokenType>& GetKeywords() {
  static std::unordered_map<std::string_view, TokenType>* keywords =
      new std::unordered_map<std::string_view, TokenType>{
          {"alias", TokenType::kAlias},
          {"break", TokenType::kBreak},
          {"case", TokenType::kCase},
          {"const", TokenType::kConst},
          {"const_assert", TokenType::kConstAssert},
          {"continue", TokenType::kContinue},
          {"continuing", TokenType::kContinuing},
          {"diagnostic", TokenType::kDiagnostic},
          {"discard", TokenType::kDiscard},
          {"default", TokenType::kDefault},
          {"else", TokenType::kElse},
          {"enable", TokenType::kEnable},
          {"fallthrough", TokenType::kFallthrough},
          {"false", TokenType::kFalse},
          {"fn", TokenType::kFn},
          {"for", TokenType::kFor},
          {"if", TokenType::kIf},
          {"let", TokenType::kLet},
          {"loop", TokenType::kLoop},
          {"override", TokenType::kOverride},
          {"return", TokenType::kReturn},
          {"requires", TokenType::kRequires},
          {"struct", TokenType::kStruct},
          {"switch", TokenType::kSwitch},
          {"true", TokenType::kTrue},
          {"var", TokenType::kVar},
          {"while", TokenType::kWhile},
          {"_", TokenType::kUnderscore},
      };

  return *keywords;
}

struct StackEntry {
  Token* token;
  size_t expr_depth;
};

void maybe_split(std::vector<Token>& tokens, size_t idx) {
  auto token = &tokens[idx];

  switch (token->type) {
    case TokenType::kShiftRight:  // >>
      token->type = TokenType::kGreaterThan;
      tokens.insert(tokens.begin() + idx, *token);
      break;
    case TokenType::kGreaterThanEqual:  // >=
      token->type = TokenType::kEqual;
      tokens.insert(tokens.begin() + idx,
                    Token{TokenType::kGreaterThan, token->line, token->column});
      break;
    case TokenType::kShiftRightEqual:  // >>=
      token->type = TokenType::kGreaterThanEqual;
      tokens.insert(tokens.begin() + idx,
                    Token{TokenType::kGreaterThan, token->line, token->column});
      break;
    default:
      break;
  }
}

void classify_template_arguments(std::vector<Token>& tokens) {
  auto count = tokens.size();

  size_t expr_depth = 0;

  std::vector<StackEntry> stack{};

  for (size_t i = 0; i < count - 1; i++) {
    switch (tokens[i].type) {
      case TokenType::kIdentifier:
      case TokenType::kVar: {
        auto& next = tokens[i + 1];
        if (next.type == TokenType::kLessThan) {
          stack.emplace_back(StackEntry{&tokens[i + 1], expr_depth});
          i++;
        }
        break;
      }
      case TokenType::kGreaterThan:       // '>'
      case TokenType::kShiftRight:        // '>>'
      case TokenType::kGreaterThanEqual:  // '>='
      case TokenType::kShiftRightEqual:   // '>>='
        if (!stack.empty() && stack.back().expr_depth == expr_depth) {
          maybe_split(tokens, i);
          auto entry = stack.back();
          stack.pop_back();

          entry.token->type = TokenType::kTemplateArgsLeft;
          tokens[i].type = TokenType::kTemplateArgsRight;
        }
        break;
      case TokenType::kParenLeft:    // (
      case TokenType::kBracketLeft:  // [
        expr_depth++;
        break;
      case TokenType::kParenRight:    // )
      case TokenType::kBracketRight:  // ]
        while (!stack.empty() && stack.back().expr_depth != expr_depth) {
          stack.pop_back();
        }

        if (expr_depth > 0) {
          expr_depth--;
        }
        break;
      case TokenType::kSemicolon:  // ;
      case TokenType::kBraceLeft:  // {
      case TokenType::kEqual:      // =
      case TokenType::kColon:      // :
        expr_depth = 0;
        stack.clear();
        break;
      case TokenType::kOrOr:    // ||
      case TokenType::kAndAnd:  // &&
        while (!stack.empty() && stack.back().expr_depth == expr_depth) {
          stack.pop_back();
        }
        break;
      default:
        break;
    }
  }
}

Scanner::Scanner(std::string_view content) : mContent(content), mCursor{1, 1} {}

std::vector<Token> Scanner::Scan() {
  std::vector<Token> tokens;
  tokens.reserve(1024);

  while (true) {
    tokens.emplace_back(Next());

    if (tokens.back().type == TokenType::kEOF ||
        tokens.back().type == TokenType::kError) {
#ifdef WGX_DEBUG_SCANNER
      if (tokens.back().type == TokenType::kError) {
        auto subStr = mContent.substr(mPos, mContent.length() - mPos);
        std::cerr << "scanner error at [ " << mPos << " ] "
                  << "rest content is : " << subStr << std::endl;
      }
#endif
      break;
    }
  }

  classify_template_arguments(tokens);

  return tokens;
}

Token Scanner::Next() {
  auto token = SkipSpaceAndComments();

  if (token.type == TokenType::kEOF || token.type == TokenType::kError) {
    return token;
  }

  auto cursor = mCursor;

  if (PeekChar() == '0' && PeekChar(1) == 'x' && IsHex(PeekChar(2))) {
    return HexNumber();
  }

  if (IsDigit(PeekChar())                             // normal number
      || (PeekChar() == '-' && IsDigit(PeekChar(1)))  // -xxx
      || (PeekChar() == '+' && IsDigit(PeekChar(1)))  // +xxx
      || (PeekChar() == '.' && IsDigit(PeekChar(1)))) {
    return Number();
  }

  if (auto t = Identity(); t.has_value()) {
    return *t;
  }

  if (auto t = Punctuation(); t.has_value()) {
    return *t;
  }

  return Token{TokenType::kError, cursor.line, cursor.column};
}

uint8_t Scanner::PeekChar(size_t offset) const {
  if (mPos + offset >= mContent.length()) {
    return '\0';
  }

  return mContent[mPos + offset];
}

bool Scanner::IsBlankSpace() const {
  auto c = PeekChar();

  return c == 0x20 || c == 0x09;
}

bool Scanner::IsLineBreak() const {
  auto c = PeekChar();

  return c == 0x0A || c == 0x0B || c == 0x0C || c == 0x0D ||
         (c == 0xC2 && PeekChar(1) == 0x85) ||
         (c == 0xE2 && PeekChar(1) == 0x80 && PeekChar(2) == 0xA8) ||
         (c == 0xE2 && PeekChar(1) == 0x80 && PeekChar(2) == 0xA9);
}

bool Scanner::IsDigit(uint8_t c) const { return std::isdigit(c); }

bool Scanner::IsHex(uint8_t c) const { return std::isxdigit(c); }

bool Scanner::IsAlphabet(uint8_t c) const {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void Scanner::Advance(size_t step) {
  mPos += step;

  mCursor.column += step;
}

void Scanner::AdvanceLine() {
  auto c = PeekChar();

  if (c == 0x0A || c == 0x0B || c == 0x0C) {
    mPos += 1;
  } else if (c == 0x0D) {
    if (PeekChar(1) != 0x0A) {
      mPos += 1;
    } else {
      mPos += 2;
    }
  } else if (c == 0xE2 && PeekChar(1) == 0x80 &&
             (PeekChar(2) == 0xA8 || PeekChar(2) == 0xA9)) {
    mPos += 3;
  }

  mCursor.line += 1;
  mCursor.column = 1;
}

Token Scanner::SkipSpaceAndComments() {
  while (true) {
    // skip empty space and line break
    while (!IsEOF()) {
      if (IsLineBreak()) {
        AdvanceLine();
        continue;
      }

      if (!IsBlankSpace()) {
        break;
      }

      Advance();
    }

    if (IsEOF()) {
      return Token(TokenType::kEOF, mCursor.line, mCursor.column);
    }

    auto comment = Comments();

    if (comment.type == TokenType::kError) {
      // meet some error
      return comment;
    }
#ifdef WGX_DEBUG_SCANNER
    if (comment.type == TokenType::kComment) {
      std::cout << "scanner skip comments: " << std::endl
                << comment.content << std::endl;
    }
#endif

    if (comment.type == TokenType::kEmpty) {
      // no comment is found
      return comment;
    }
  }
}

bool Scanner::Match(std::string_view str) const {
  if (mPos + str.length() >= mContent.length()) {
    return false;
  }

  return mContent.substr(mPos, str.length()) == str;
}

Token Scanner::Comments() {
  auto start = mPos;
  auto end = start;
  if (Match("//")) {
    // Line comment: ignore everything until the end of line
    while (!IsLineBreak()) {
      Advance();
    }

    end = mPos;
    if (IsLineBreak()) {
      AdvanceLine();
    }
  }

  if (Match("/*")) {
    // Block comment: ignore everything until the closing '*/'.

    // skip the first '/*'
    Advance(2);

    int32_t depth = 1;
    while (!IsEOF() && depth > 0) {
      if (Match("/*")) {
        // nested block comments
        Advance(2);
        depth++;
      } else if (Match("*/")) {
        Advance(2);
        depth--;
      } else if (IsLineBreak()) {
        AdvanceLine();
      } else {
        Advance();
      }
    }

    if (depth > 0) {
      return Token{TokenType::kError, mContent, "Unterminated block comment",
                   mCursor.line, mCursor.column};
    }
    end = mPos;
  }

  if (IsEOF()) {
    return Token(TokenType::kEOF, mContent, mCursor.line, mCursor.column);
  } else if (start == end) {
    return Token(TokenType::kEmpty, mContent, mCursor.line, mCursor.column);
  } else {
    return {TokenType::kComment, mContent.substr(start, end - start),
            mContent.substr(start, end - start), mCursor.line, mCursor.column};
  }
}

Token Scanner::HexNumber() {
  auto cursor = mCursor;

  // step over prefix: '0x'
  Advance(2);

  auto start = mPos;
  while (IsHex(PeekChar())) {
    Advance();
  }

  auto end = mPos;

  int64_t value = 0;

  auto res = std::from_chars(mContent.data() + start, mContent.data() + end,
                             value, 16);

  if (res.ec == std::errc()) {
    return Token{TokenType::kIntLiteral, mContent.substr(start, end - start),
                 value, cursor.line, cursor.column};
  }
  return Token{TokenType::kError, cursor.line, cursor.column};
}

Token Scanner::Number() {
  bool negative = false;
  bool is_float = false;
  int32_t div_pow = -1;

  auto cursor = mCursor;

  if (PeekChar() == '-') {
    negative = true;
    Advance();
  }

  if (PeekChar() == '.') {
    is_float = true;
    div_pow = 0;
    Advance();
  }

  auto start = mPos;

  while (IsDigit(PeekChar())) {
    Advance();

    if (PeekChar() == '.') {
      if (is_float) {
        return Token{TokenType::kError, cursor.line, cursor.column};
      }

      if (!IsDigit(PeekChar(1))) {
        return Token{TokenType::kError, cursor.line, cursor.column};
      }

      is_float = true;
      Advance();
    }

    if (div_pow >= 0) {
      div_pow++;
    }
  }

  auto end = mPos;

  if (end == start) {
    return Token{TokenType::kError, cursor.line, cursor.column};
  }

  if (is_float) {
    double value =
        std::stod(std::string{mContent.data() + start, mContent.data() + end});

    if (div_pow > 0) {
      auto div = std::pow(10, div_pow);
      value /= div;
    }

    if (negative) {
      value *= -1.0;
    }

    return Token{TokenType::kFloatLiteral, mContent.substr(start, end - start),
                 value, cursor.line, cursor.column};
  } else {
    int64_t value = 0;
    auto res =
        std::from_chars(mContent.data() + start, mContent.data() + end, value);
    if (negative) {
      value *= -1;
    }

    if (res.ec == std::errc()) {
      return Token{TokenType::kIntLiteral, mContent.substr(start, end - start),
                   value, cursor.line, cursor.column};
    }
  }

  return Token{TokenType::kError, cursor.line, cursor.column};
}

std::optional<Token> Scanner::Identity() {
  if (!IsAlphabet(PeekChar())) {
    return std::nullopt;
  }

  auto cursor = mCursor;

  auto start = mPos;
  while (IsAlphabet(PeekChar()) || IsDigit(PeekChar())) {
    Advance();
  }

  auto end = mPos;

  if (start == end) {
    return std::nullopt;
  }

  auto str = mContent.substr(start, end - start);

  const auto& key_words_map = GetKeywords();
  auto ki = key_words_map.find(str);

  if (ki != key_words_map.end()) {
    return Token{ki->second, cursor.line, cursor.column};
  }

  return Token{TokenType::kIdentifier, str, str, cursor.line, cursor.column};
}

std::optional<Token> Scanner::Punctuation() {
  auto type = TokenType::kEmpty;

  auto cursor = mCursor;

  if (PeekChar() == '@') {
    type = TokenType::kAttr;
    Advance();
  } else if (PeekChar() == '(') {
    type = TokenType::kParenLeft;
    Advance();
  } else if (PeekChar() == ')') {
    type = TokenType::kParenRight;
    Advance();
  } else if (PeekChar() == '[') {
    type = TokenType::kBracketLeft;
    Advance();
  } else if (PeekChar() == ']') {
    type = TokenType::kBracketRight;
    Advance();
  } else if (PeekChar() == '{') {
    type = TokenType::kBraceLeft;
    Advance();
  } else if (PeekChar() == '}') {
    type = TokenType::kBraceRight;
    Advance();
  } else if (PeekChar() == '&') {
    if (PeekChar(1) == '&') {
      type = TokenType::kAndAnd;
      Advance(2);
    } else if (PeekChar(1) == '=') {
      type = TokenType::kAndEqual;
      Advance(2);
    } else {
      type = TokenType::kAnd;
      Advance();
    }
  } else if (PeekChar() == '/') {
    if (PeekChar(1) == '=') {
      type = TokenType::kDivisionEqual;
      Advance(2);
    } else {
      type = TokenType::kForwardSlash;
      Advance();
    }
  } else if (PeekChar() == '!') {
    if (PeekChar(1) == '=') {
      type = TokenType::kNotEqual;
      Advance(2);
    } else {
      type = TokenType::kBang;
      Advance();
    }
  } else if (PeekChar() == ':') {
    type = TokenType::kColon;
    Advance();
  } else if (PeekChar() == ',') {
    type = TokenType::kComma;
    Advance();
  } else if (PeekChar() == '=') {
    if (PeekChar(1) == '=') {
      type = TokenType::kEqualEqual;
      Advance(2);
    } else {
      type = TokenType::kEqual;
      Advance();
    }
  } else if (PeekChar() == '>') {
    if (PeekChar(1) == '=') {
      type = TokenType::kGreaterThanEqual;
      Advance(2);
    } else if (PeekChar(1) == '>') {
      if (PeekChar(2) == '=') {
        type = TokenType::kShiftRightEqual;
        Advance(3);
      } else {
        type = TokenType::kShiftRight;
        Advance(2);
      }
    } else {
      type = TokenType::kGreaterThan;
      Advance();
    }
  } else if (PeekChar() == '<') {
    if (PeekChar(1) == '=') {
      type = TokenType::kLessThanEqual;
      Advance(2);
    } else if (PeekChar(1) == '<') {
      if (PeekChar(2) == '=') {
        type = TokenType::kShiftLeftEqual;
        Advance(3);
      } else {
        type = TokenType::kShiftLeft;
        Advance(2);
      }
    } else {
      type = TokenType::kLessThan;
      Advance();
    }
  } else if (PeekChar() == '%') {
    if (PeekChar(1) == '=') {
      type = TokenType::kModuloEqual;
      Advance(2);
    } else {
      type = TokenType::kMod;
      Advance();
    }
  } else if (PeekChar() == '-') {
    if (PeekChar(1) == '>') {
      type = TokenType::kArrow;
      Advance(2);
    } else if (PeekChar(1) == '-') {
      type = TokenType::kMinusMinus;
      Advance(2);
    } else if (PeekChar(1) == '=') {
      type = TokenType::kMinusEqual;
      Advance(2);
    } else {
      type = TokenType::kMinus;
      Advance();
    }
  } else if (PeekChar() == '.') {
    type = TokenType::kPeriod;
    Advance();
  } else if (PeekChar() == '+') {
    if (PeekChar(1) == '+') {
      type = TokenType::kPlusPlus;
      Advance(2);
    } else if (PeekChar(1) == '=') {
      type = TokenType::kPlusEqual;
      Advance(2);
    } else {
      type = TokenType::kPlus;
      Advance();
    }
  } else if (PeekChar() == '|') {
    if (PeekChar(1) == '|') {
      type = TokenType::kOrOr;
      Advance(2);
    } else if (PeekChar(1) == '=') {
      type = TokenType::kOrEqual;
      Advance(2);
    } else {
      type = TokenType::kOr;
      Advance();
    }
  } else if (PeekChar() == ';') {
    type = TokenType::kSemicolon;
    Advance();
  } else if (PeekChar() == '*') {
    if (PeekChar(1) == '=') {
      type = TokenType::kTimesEqual;
      Advance(2);
    } else {
      type = TokenType::kStar;
      Advance();
    }
  } else if (PeekChar() == '~') {
    type = TokenType::kTilde;
    Advance();
  } else if (PeekChar() == '_') {
    type = TokenType::kUnderscore;
    Advance();
  } else if (PeekChar() == '^') {
    if (PeekChar(1) == '=') {
      type = TokenType::kXorEqual;
      Advance(2);
    } else {
      type = TokenType::kXor;
      Advance();
    }
  }

  if (type == TokenType::kEmpty) {
    return std::nullopt;
  } else {
    return Token{type, cursor.line, cursor.column};
  }
}

}  // namespace wgx
