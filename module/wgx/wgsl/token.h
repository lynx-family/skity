/*
 * This file contains code portions derived from [Dawn & Tint], which is
 * licensed under the BSD 3-Clause License.
 *
 * The original source code can be found at [google/dawn project].
 *
 * BSD 3-Clause License:
 *
 * Redistribution and use in part or in whole of the code in this file,
 * subject to the following conditions:
 *
 * 1. Redistributions must include the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions must not modify the copyright notice, the list
 *    of conditions or the above disclaimer.
 *
 * 3. Redistributions must include a notice like this one in all
 *    reproductions or derivative works.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING BUT NOT
 * LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * IMPOSSIBLE, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING
 * BUT NOT LIMITED TO, LOSS OF USE, LOSS OF DATA, LOSS OF PROFITS,
 * OR BUSINESS INTERVALS) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, OR IN ANY WAY OUT OF THE COPYING OR REPRODUCTION OF
 * THIS SOFTWARE, ARE DISCLAIMED.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <string_view>
#include <variant>

namespace wgx {

enum class TokenType {
  kEmpty,
  kError,
  kEOF,

  /// A comment, can be single line comment, or block comments
  kComment,
  /// An identifier
  kIdentifier,
  /// A float literal with no suffix
  kFloatLiteral,
  /// A float literal with an 'f' suffix
  kFloatLiteral_F,
  /// A float literal with an 'h' suffix
  kFloatLiteral_H,
  /// An integer literal with no suffix
  kIntLiteral,
  /// An integer literal with an 'i' suffix
  kIntLiteral_I,
  /// An integer literal with a 'u' suffix
  kIntLiteral_U,

  /// A '&'
  kAnd,
  /// A '&&'
  kAndAnd,
  /// A '->'
  kArrow,
  /// A '@'
  kAttr,
  /// A '/'
  kForwardSlash,
  /// A '!'
  kBang,
  /// A '['
  kBracketLeft,
  /// A ']'
  kBracketRight,
  /// A '{'
  kBraceLeft,
  /// A '}'
  kBraceRight,
  /// A ':'
  kColon,
  /// A ','
  kComma,
  /// A '='
  kEqual,
  /// A '=='
  kEqualEqual,
  /// A '>' (post template-args classification)
  kTemplateArgsRight,
  /// A '>'
  kGreaterThan,
  /// A '>='
  kGreaterThanEqual,
  /// A '>>'
  kShiftRight,
  /// A '<' (post template-args classification)
  kTemplateArgsLeft,
  /// A '<'
  kLessThan,
  /// A '<='
  kLessThanEqual,
  /// A '<<'
  kShiftLeft,
  /// A '%'
  kMod,
  /// A '-'
  kMinus,
  /// A '--'
  kMinusMinus,
  /// A '!='
  kNotEqual,
  /// A '.'
  kPeriod,
  /// A '+'
  kPlus,
  /// A '++'
  kPlusPlus,
  /// A '|'
  kOr,
  /// A '||'
  kOrOr,
  /// A '('
  kParenLeft,
  /// A ')'
  kParenRight,
  /// A ';'
  kSemicolon,
  /// A '*'
  kStar,
  /// A '~'
  kTilde,
  /// A '_'
  kUnderscore,
  /// A '^'
  kXor,
  /// A '+='
  kPlusEqual,
  /// A '-='
  kMinusEqual,
  /// A '*='
  kTimesEqual,
  /// A '/='
  kDivisionEqual,
  /// A '%='
  kModuloEqual,
  /// A '&='
  kAndEqual,
  /// A '|='
  kOrEqual,
  /// A '^='
  kXorEqual,
  /// A '>>='
  kShiftRightEqual,
  /// A '<<='
  kShiftLeftEqual,

  /// A 'alias'
  kAlias,
  /// A 'break'
  kBreak,
  /// A 'case'
  kCase,
  /// A 'const'
  kConst,
  /// A 'const_assert'
  kConstAssert,
  /// A 'continue'
  kContinue,
  /// A 'continuing'
  kContinuing,
  /// A 'default'
  kDefault,
  /// A 'diagnostic'
  kDiagnostic,
  /// A 'discard'
  kDiscard,
  /// A 'else'
  kElse,
  /// A 'enable'
  kEnable,
  /// A 'fallthrough'
  // Note, this isn't a keyword, but a reserved word. We match it as a keyword
  // in order to provide better diagnostics in case a `fallthrough` is added to
  // a case body.
  kFallthrough,
  /// A 'false'
  kFalse,
  /// A 'fn'
  kFn,
  // A 'for'
  kFor,
  /// A 'if'
  kIf,
  /// A 'let'
  kLet,
  /// A 'loop'
  kLoop,
  /// A 'override'
  kOverride,
  /// A 'requires'
  kRequires,
  /// A 'return'
  kReturn,
  /// A 'struct'
  kStruct,
  /// A 'switch'
  kSwitch,
  /// A 'true'
  kTrue,
  /// A 'var'
  kVar,
  /// A 'while'
  kWhile,
};

struct Token {
  TokenType type;
  std::string_view content;
  std::variant<int64_t, double, std::string_view> value;
  size_t line;
  size_t column;

  explicit Token(TokenType type, size_t line, size_t column)
      : type(type), content(), value(), line(line), column(column) {}

  Token(TokenType type, std::string_view content, size_t line, size_t column)
      : type(type), content(content), value(), line(line), column(column) {}

  template <class T>
  Token(TokenType type, std::string_view content, T value, size_t line,
        size_t column)
      : type(type),
        content(content),
        value(value),
        line(line),
        column(column) {}

  bool Is(TokenType t) const { return type == t; }

  double ToF64() const;

  int64_t ToI64() const;

  std::string_view ToString() const;
};

}  // namespace wgx