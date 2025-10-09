/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_PLATFORM_WIN_LEAN_WINDOWS_HPP
#define SRC_BASE_PLATFORM_WIN_LEAN_WINDOWS_HPP

// https://devblogs.microsoft.com/oldnewthing/20091130-00/?p=15863
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define WIN32_IS_MEAN_WAS_LOCALLY_DEFINED
#endif
#ifndef NOMINMAX
#define NOMINMAX
#define NOMINMAX_WAS_LOCALLY_DEFINED
#endif

#include <windows.h>

#ifdef WIN32_IS_MEAN_WAS_LOCALLY_DEFINED
#undef WIN32_IS_MEAN_WAS_LOCALLY_DEFINED
#undef WIN32_LEAN_AND_MEAN
#endif
#ifdef NOMINMAX_WAS_LOCALLY_DEFINED
#undef NOMINMAX_WAS_LOCALLY_DEFINED
#undef NOMINMAX
#endif

#endif  // SRC_BASE_PLATFORM_WIN_LEAN_WINDOWS_HPP
