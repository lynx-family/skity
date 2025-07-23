// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_UTILS_ANNOTATION_MUTEX_HPP
#define SRC_UTILS_ANNOTATION_MUTEX_HPP

#include <skity/macros.hpp>

#ifndef SKITY_IOS
#include <shared_mutex>
#endif
#include <cassert>

#include "src/utils/thread_annotations.hpp"

namespace skity {

/**
 * Utility classes wrap std mutex and lock with clang annotations
 */
class SKITY_CAPABILITY("mutex") SharedMutex {
 public:
  ~SharedMutex() = default;

#ifndef SKITY_IOS
  SharedMutex() = default;

  void Lock() SKITY_ACQUIRE() { mutex_.lock(); }

  void LockShared() SKITY_ACQUIRE_SHARED() { mutex_.lock_shared(); }

  void Unlock() SKITY_RELEASE_CAPABILITY() { mutex_.unlock(); }

  void UnlockShared() SKITY_RELEASE_SHARED_CAPABILITY() {
    mutex_.unlock_shared();
  }

 private:
  std::shared_mutex mutex_;
#else
  SharedMutex() {
    int ret = pthread_rwlock_init(&rwlock_, nullptr);
    if (ret != 0) {
      assert(ret);
    }
  }

  void Lock() SKITY_ACQUIRE() { pthread_rwlock_wrlock(&rwlock_); }

  void LockShared() SKITY_ACQUIRE_SHARED() { pthread_rwlock_rdlock(&rwlock_); }

  void Unlock() SKITY_RELEASE_CAPABILITY() { pthread_rwlock_unlock(&rwlock_); }

  void UnlockShared() SKITY_RELEASE_SHARED_CAPABILITY() {
    pthread_rwlock_unlock(&rwlock_);
  }

 private:
  pthread_rwlock_t rwlock_;
#endif

  SharedMutex(const SharedMutex&) = delete;
  SharedMutex(SharedMutex&&) = delete;
  SharedMutex& operator=(const SharedMutex&) = delete;
  SharedMutex& operator=(SharedMutex&&) = delete;
};

class SKITY_SCOPED_CAPABILITY SharedLock {
 public:
  explicit SharedLock(SharedMutex& mutex) SKITY_ACQUIRE_SHARED(mutex)
      : mutex_(mutex) {
    mutex_.LockShared();
  }

  ~SharedLock() SKITY_RELEASE_CAPABILITY() { mutex_.UnlockShared(); }

 private:
  SharedMutex& mutex_;

  SharedLock(const SharedLock&) = delete;
  SharedLock(SharedLock&&) = delete;
  SharedLock& operator=(const SharedLock&) = delete;
  SharedLock& operator=(SharedLock&&) = delete;
};

class SKITY_SCOPED_CAPABILITY UniqueLock {
 public:
  explicit UniqueLock(SharedMutex& mutex) SKITY_ACQUIRE(mutex) : mutex_(mutex) {
    mutex_.Lock();
  }

  ~UniqueLock() SKITY_RELEASE_CAPABILITY() { mutex_.Unlock(); }

 private:
  SharedMutex& mutex_;

  UniqueLock(const UniqueLock&) = delete;
  UniqueLock(UniqueLock&&) = delete;
  UniqueLock& operator=(const UniqueLock&) = delete;
  UniqueLock& operator=(UniqueLock&&) = delete;
};

}  // namespace skity

#endif  // SRC_UTILS_ANNOTATION_MUTEX_HPP
