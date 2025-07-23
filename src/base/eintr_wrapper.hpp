// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_EINTR_WRAPPER_HPP
#define SRC_BASE_EINTR_WRAPPER_HPP

#include <errno.h>

#ifdef SKITY_WIN

// Windows has no concept of EINTR.
#define SKITY_HANDLE_EINTR(x) (x)
#define SKITY_IGNORE_EINTR(x) (x)

#else

#define SKITY_HANDLE_EINTR(x)                               \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

#define SKITY_IGNORE_EINTR(x)                             \
  ({                                                      \
    decltype(x) eintr_wrapper_result;                     \
    do {                                                  \
      eintr_wrapper_result = (x);                         \
      if (eintr_wrapper_result == -1 && errno == EINTR) { \
        eintr_wrapper_result = 0;                         \
      }                                                   \
    } while (0);                                          \
    eintr_wrapper_result;                                 \
  })

#endif  // SKITY_WIN

#endif  // SRC_BASE_EINTR_WRAPPER_HPP
