/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_THREAD_ANNOTATIONS_HPP
#define SRC_UTILS_THREAD_ANNOTATIONS_HPP

// The bulk of this code is cribbed from:
// http://clang.llvm.org/docs/ThreadSafetyAnalysis.html

// #if defined(__clang__) && (!defined(SWIG))
// #define SKITY_THREAD_ANNOTATION_ATTRIBUTE(x) __attribute__((x))
// #else
#define SKITY_THREAD_ANNOTATION_ATTRIBUTE(x)  // no-op
// #endif

#define SKITY_CAPABILITY(x) SKITY_THREAD_ANNOTATION_ATTRIBUTE(capability(x))

#define SKITY_SCOPED_CAPABILITY \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(scoped_lockable)

#define SKITY_GUARDED_BY(x) SKITY_THREAD_ANNOTATION_ATTRIBUTE(guarded_by(x))

#define SKITY_PT_GUARDED_BY(x) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(pt_guarded_by(x))

#define SKITY_ACQUIRED_BEFORE(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(acquired_before(__VA_ARGS__))

#define SKITY_ACQUIRED_AFTER(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(acquired_after(__VA_ARGS__))

#define SKITY_REQUIRES(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(requires_capability(__VA_ARGS__))

#define SKITY_REQUIRES_SHARED(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(requires_shared_capability(__VA_ARGS__))

#define SKITY_ACQUIRE(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(acquire_capability(__VA_ARGS__))

#define SKITY_ACQUIRE_SHARED(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(acquire_shared_capability(__VA_ARGS__))

// Would be SKITY_RELEASE, but that is already in use as SKITY_DEBUG vs.
// SKITY_RELEASE.
#define SKITY_RELEASE_CAPABILITY(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(release_capability(__VA_ARGS__))

// For symmetry with SKITY_RELEASE_CAPABILITY.
#define SKITY_RELEASE_SHARED_CAPABILITY(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(release_shared_capability(__VA_ARGS__))

#define SKITY_TRY_ACQUIRE(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(try_acquire_capability(__VA_ARGS__))

#define SKITY_TRY_ACQUIRE_SHARED(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(try_acquire_shared_capability(__VA_ARGS__))

#define SKITY_EXCLUDES(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(locks_excluded(__VA_ARGS__))

#define SKITY_ASSERT_CAPABILITY(x) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(assert_capability(x))

#define SKITY_ASSERT_SHARED_CAPABILITY(x) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(assert_shared_capability(x))

#define SKITY_RETURN_CAPABILITY(x) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(lock_returned(x))

#define SKITY_NO_THREAD_SAFETY_ANALYSIS \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(no_thread_safety_analysis)

#define SKITY_LOCKABLE SKITY_THREAD_ANNOTATION_ATTRIBUTE(lockable)

#define SKITY_SCOPED_LOCKABLE SKITY_THREAD_ANNOTATION_ATTRIBUTE(scoped_lockable)

#define SKITY_EXCLUSIVE_LOCK_FUNCTION(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(exclusive_lock_function(__VA_ARGS__))

#define SKITY_SHARED_LOCK_FUNCTION(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(shared_lock_function(__VA_ARGS__))

#define SKITY_ASSERT_EXCLUSIVE_LOCK(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(assert_exclusive_lock(__VA_ARGS__))

#define SKITY_ASSERT_SHARED_LOCK(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(assert_shared_lock(__VA_ARGS__))

#define SKITY_EXCLUSIVE_TRYLOCK_FUNCTION(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(exclusive_trylock_function(__VA_ARGS__))

#define SKITY_SHARED_TRYLOCK_FUNCTION(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(shared_trylock_function(__VA_ARGS__))

#define SKITY_UNLOCK_FUNCTION(...) \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(unlock_function(__VA_ARGS__))

#define NO_THREAD_SAFETY_ANALYSIS \
  SKITY_THREAD_ANNOTATION_ATTRIBUTE(no_thread_safety_analysis)

#if defined(SKITY_BUILD_FOR_GOOGLE3) && \
    !defined(SKITY_BUILD_FOR_WASM_IN_GOOGLE3)
extern "C" {
void __google_potentially_blocking_region_begin(void);
void __google_potentially_blocking_region_end(void);
}
#define SKITY_POTENTIALLY_BLOCKING_REGION_BEGIN \
  __google_potentially_blocking_region_begin()
#define SKITY_POTENTIALLY_BLOCKING_REGION_END \
  __google_potentially_blocking_region_end()
#else
#define SKITY_POTENTIALLY_BLOCKING_REGION_BEGIN
#define SKITY_POTENTIALLY_BLOCKING_REGION_END
#endif

#endif  // SRC_UTILS_THREAD_ANNOTATIONS_HPP
