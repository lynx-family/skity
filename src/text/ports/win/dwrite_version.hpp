/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_PORTS_WIN_DWRITE_VERSION_HPP
#define SRC_TEXT_PORTS_WIN_DWRITE_VERSION_HPP

// More strictly, this header should be the first thing in a translation unit,
// since it is effectively negating build flags.
#if defined(_WINDOWS_) || defined(DWRITE_3_H_INCLUDED)
#error Must include DWriteNTDDI_VERSION.h before any Windows or DWrite headers.
#endif

// If the build defines NTDDI_VERSION, pretend it didn't.
// This also requires resetting _WIN32_WINNT and WINVER.
// dwrite_3.h guards enum, macro, and interface declarations behind
// NTDDI_VERSION, but it is not clear this is correct since these are all
// immutable.
#if defined(NTDDI_VERSION)
#undef NTDDI_VERSION
#if defined(_WIN32_WINNT)
#undef _WIN32_WINNT
#endif
#if defined(WINVER)
#undef WINVER
#endif
#endif

#endif  // SRC_TEXT_PORTS_WIN_DWRITE_VERSION_HPP
