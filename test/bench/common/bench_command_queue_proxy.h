// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import <Metal/Metal.h>

NS_ASSUME_NONNULL_BEGIN

@interface CommandQueueProxy : NSObject <MTLCommandQueue>

- (instancetype)initWithRealCommandQueue:(id<MTLCommandQueue>)realQueue;

@property(nonatomic, strong, readonly) id<MTLCommandQueue> realQueue;

- (BOOL)waitTillFinished;

@end

NS_ASSUME_NONNULL_END
