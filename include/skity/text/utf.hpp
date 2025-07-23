// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_TEXT_UTF_HPP
#define INCLUDE_SKITY_TEXT_UTF_HPP

#include <cstddef>
#include <cstdint>
#include <skity/macros.hpp>
#include <skity/text/typeface.hpp>
#include <vector>

namespace skity {

/**
 * @class UTF
 * utf8 convert helper
 */
class SKITY_API UTF final {
 public:
  UTF() = delete;
  UTF& operator=(const UTF&) = delete;
  ~UTF() = delete;

  /**
   * Given a sequence of UTF-8 bytes, return the number of unicode codepoints.
   *
   * @param byte_length	length of utf8 string
   * @return            number of unicode codepoint or -1 if invalid.
   */
  static int32_t CountUTF8(const char* text, size_t byte_length);

  /**
   * Given a sequence of aligned UTF-8 characters in machine-endian form, return
   * the first unicode codepoint.
   */
  static int32_t NextUTF8(const char** ptr, const char* end);

  static bool UTF8ToCodePoint(const char* text, size_t byte_length,
                              std::vector<Unichar>& code_points);
  /**
   * Convert the unicode codepoint into UTF-16.
   *
   * @param ch      unicode char
   * @param utf16   utf16 result
   * @return size_t  the number of UTF-16 code units in the result
   */
  static size_t ConvertToUTF16(Unichar ch, uint16_t utf16[2]);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_TEXT_UTF_HPP
