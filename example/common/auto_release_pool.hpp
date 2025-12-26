// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if defined(__APPLE__)
#include <objc/objc.h>
#include <objc/runtime.h>
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void*);
#endif

class AutoReleasePool {
 public:
  AutoReleasePool() {
#if defined(__APPLE__)
    pool_ = objc_autoreleasePoolPush();
#endif
  }

  ~AutoReleasePool() {
#if defined(__APPLE__)
    objc_autoreleasePoolPop(pool_);
#endif
  }

 private:
  void* pool_ = nullptr;
};
