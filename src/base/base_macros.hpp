// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_BASE_BASE_MACROS_HPP
#define SRC_BASE_BASE_MACROS_HPP

#define SKITY_DISALLOW_COPY(TypeName) TypeName(const TypeName&) = delete

#define SKITY_DISALLOW_ASSIGN(TypeName) \
  TypeName& operator=(const TypeName&) = delete

#define SKITY_DISALLOW_MOVE(TypeName) \
  TypeName(TypeName&&) = delete;      \
  TypeName& operator=(TypeName&&) = delete

#define SKITY_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;            \
  TypeName& operator=(const TypeName&) = delete

#define SKITY_DISALLOW_COPY_ASSIGN_AND_MOVE(TypeName) \
  TypeName(const TypeName&) = delete;                 \
  TypeName(TypeName&&) = delete;                      \
  TypeName& operator=(const TypeName&) = delete;      \
  TypeName& operator=(TypeName&&) = delete

#define SKITY_DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName() = delete;                                 \
  SKITY_DISALLOW_COPY_ASSIGN_AND_MOVE(TypeName)

#endif  // SRC_BASE_BASE_MACROS_HPP
