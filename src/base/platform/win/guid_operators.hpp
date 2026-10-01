// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_PLATFORM_WIN_GUID_OPERATORS_HPP
#define SRC_BASE_PLATFORM_WIN_GUID_OPERATORS_HPP

#include <guiddef.h>

// guiddef.h normally declares these inline operators:
//
//   __inline bool operator==(REFGUID guidOne, REFGUID guidOther);
//   __inline bool operator!=(REFGUID guidOne, REFGUID guidOther);
//
// MSVC 14.4x publishes the COMDAT of that inline function under the malformed
// symbol name "==" instead of the decorated "??8@YA_NAEBU_GUID@@0@Z". With
// CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS, CMake turns every COMDAT name into an
// export entry and strips the leading character, so the generated module
// definition file ends up with a bare "=" and the link fails with:
//
//   exports.def : error LNK2001: unresolved external symbol =
//   skity.lib : fatal error LNK1120: 1 unresolved externals
//
// Defining _SYS_GUID_OPERATOR_EQ_ makes guiddef.h skip its declaration, and
// this header supplies equivalent operators with internal linkage instead. They
// compile to ordinary local symbols, keep GUID comparison working, and never
// reach the export list.
//
// The macro cannot be defined in this header: guiddef.h is reached through
// <windows.h>, which dwrite_version.hpp requires to come last, so the
// definition has to precede the first include of the translation unit. The
// four files that compare GUID values therefore define it at the very top and
// reach this header through dwrite_utils.hpp / scaler_context_win.hpp /
// typeface_win.hpp. A new translation unit comparing GUID values needs the
// same two lines plus one of those includes.

// Same semantics as the operators guiddef.h would have provided.
static inline bool operator==(REFGUID guid_one, REFGUID guid_other) {
  return IsEqualGUID(guid_one, guid_other) != 0;
}

static inline bool operator!=(REFGUID guid_one, REFGUID guid_other) {
  return IsEqualGUID(guid_one, guid_other) == 0;
}

#endif  // SRC_BASE_PLATFORM_WIN_GUID_OPERATORS_HPP
