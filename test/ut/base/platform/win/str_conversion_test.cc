// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/base/platform/win/str_conversion.hpp"

#include <gtest/gtest.h>

static void compare_conversion_result(const std::string& s,
                                      const std::wstring& w_s) {
  std::wstring s_convert_result;
  skity::StrConversion::StringToWideString(s, &s_convert_result);

  std::string w_s_convert_result;
  skity::StrConversion::WideStringToString(w_s, &w_s_convert_result);

  EXPECT_EQ(s, w_s_convert_result);
  EXPECT_EQ(w_s, s_convert_result);
}

TEST(STR_CONVERSION, StringToWideString) {
  compare_conversion_result("", L"");
  compare_conversion_result("skity", L"skity");
  compare_conversion_result("\xE4\xBD\xA0\xE5\xA5\xBD", L"\x4F60\x597D");
}
