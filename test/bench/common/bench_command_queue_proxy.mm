// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#import "test/bench/common/bench_command_queue_proxy.h"

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#include "test/bench/common/bench_gpu_time_tracer.hpp"

@interface CommandQueueProxy ()

@property(nonatomic, strong) NSMutableArray<id<MTLCommandBuffer>> *commandBuffers;

@end

@implementation CommandQueueProxy

@synthesize label = _label;

- (instancetype)initWithRealCommandQueue:(id<MTLCommandQueue>)realQueue {
  self = [super init];
  if (self) {
    _realQueue = realQueue;
    _label = realQueue.label;
    _commandBuffers = [NSMutableArray array];
    skity::BenchGPUTimeTracer::Instance();
    __weak CommandQueueProxy *weakSelf = self;
    skity::BenchGPUTimeTracer::Instance().SetCallback(
        [weakSelf] {
          if (weakSelf == nil) {
            return;
          }
          __strong CommandQueueProxy *strongSelf = weakSelf;
          [strongSelf waitTillFinished];
          [strongSelf.commandBuffers removeAllObjects];
        },
        [weakSelf] {
          if (weakSelf == nil) {
            return;
          }
          __strong CommandQueueProxy *strongSelf = weakSelf;
          [strongSelf waitTillFinished];
        });
  }
  return self;
}

- (void)setLabel:(NSString *)label {
  _label = [label copy];
  self.realQueue.label = label;
}

- (NSString *)label {
  return _label ?: self.realQueue.label;
}

- (id<MTLDevice>)device {
  return [self.realQueue device];
}

- (id<MTLCommandBuffer>)commandBuffer {
  id<MTLCommandBuffer> buffer = [self.realQueue commandBuffer];
  [self.commandBuffers addObject:buffer];
  if (skity::BenchGPUTimeTracer::Instance().IsTracing()) {
    int32_t frame_index = skity::BenchGPUTimeTracer::Instance().CurrentFrameIndex();
    [buffer addCompletedHandler:^(id<MTLCommandBuffer> cmdBuffer) {
      skity::GPUTimeDuration duration;
      skity::BenchGPUTimeTracer::Instance().AppendGPUFrameTime(frame_index, cmdBuffer.GPUStartTime,
                                                               cmdBuffer.GPUEndTime);
    }];
  }
  return buffer;
}

- (id<MTLCommandBuffer>)commandBufferWithDescriptor:(MTLCommandBufferDescriptor *)descriptor {
  id<MTLCommandBuffer> buffer = nil;
  if ([self.realQueue respondsToSelector:@selector(commandBufferWithDescriptor:)]) {
    buffer = [self.realQueue commandBufferWithDescriptor:descriptor];
    [self.commandBuffers addObject:buffer];
    if (skity::BenchGPUTimeTracer::Instance().IsTracing()) {
      int32_t frame_index = skity::BenchGPUTimeTracer::Instance().CurrentFrameIndex();
      [buffer addCompletedHandler:^(id<MTLCommandBuffer> cmdBuffer) {
        skity::GPUTimeDuration duration;
        skity::BenchGPUTimeTracer::Instance().AppendGPUFrameTime(
            frame_index, cmdBuffer.GPUStartTime, cmdBuffer.GPUEndTime);
      }];
    }
  }
  return buffer;
}

- (id<MTLCommandBuffer>)commandBufferWithUnretainedReferences {
  id<MTLCommandBuffer> buffer = [self.realQueue commandBufferWithUnretainedReferences];

  if (skity::BenchGPUTimeTracer::Instance().IsTracing()) {
    int32_t frame_index = skity::BenchGPUTimeTracer::Instance().CurrentFrameIndex();
    [self.commandBuffers addObject:buffer];
    [buffer addCompletedHandler:^(id<MTLCommandBuffer> cmdBuffer) {
      skity::GPUTimeDuration duration;
      skity::BenchGPUTimeTracer::Instance().AppendGPUFrameTime(frame_index, cmdBuffer.GPUStartTime,
                                                               cmdBuffer.GPUEndTime);
    }];
  }
  return buffer;
}

- (void)insertDebugCaptureBoundary {
  if ([self.realQueue respondsToSelector:@selector(insertDebugCaptureBoundary)]) {
    [self.realQueue insertDebugCaptureBoundary];
  }
}

- (BOOL)waitTillFinished {
  if (self.commandBuffers.count > 0) {
    CFAbsoluteTime start = CFAbsoluteTimeGetCurrent();
    CFAbsoluteTime end = start;
    for (id<MTLCommandBuffer> buffer in self.commandBuffers) {
      while (buffer.status != MTLCommandBufferStatusCompleted) {
        end = CFAbsoluteTimeGetCurrent();
        assert(buffer.status != MTLCommandBufferStatusError);
        assert(end - start < 5);
      }
    }
  }
}

@end
