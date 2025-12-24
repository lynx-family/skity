// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#if !__has_feature(objc_arc)
#error ARC must be enabled!
#endif

#include "test/bench/common/bench_gl_context_mac.h"

#include <AppKit/AppKit.h>
#include <OpenGL/OpenGL.h>
#include <memory>

namespace skity {

namespace {
static inline NSOpenGLPixelFormat *GetGLPixelFormat(int sampleCount) {
  const int kMaxAttributes = 19;
  NSOpenGLPixelFormatAttribute attributes[kMaxAttributes];
  int numAttributes = 0;
  attributes[numAttributes++] = NSOpenGLPFAAccelerated;
  attributes[numAttributes++] = NSOpenGLPFAClosestPolicy;
  attributes[numAttributes++] = NSOpenGLPFADoubleBuffer;
  attributes[numAttributes++] = NSOpenGLPFAOpenGLProfile;
  attributes[numAttributes++] = NSOpenGLProfileVersion3_2Core;
  attributes[numAttributes++] = NSOpenGLPFAColorSize;
  attributes[numAttributes++] = 24;
  attributes[numAttributes++] = NSOpenGLPFAAlphaSize;
  attributes[numAttributes++] = 8;
  attributes[numAttributes++] = NSOpenGLPFADepthSize;
  attributes[numAttributes++] = 0;
  attributes[numAttributes++] = NSOpenGLPFAStencilSize;
  attributes[numAttributes++] = 8;
  if (sampleCount > 1) {
    attributes[numAttributes++] = NSOpenGLPFAMultisample;
    attributes[numAttributes++] = NSOpenGLPFASampleBuffers;
    attributes[numAttributes++] = 1;
    attributes[numAttributes++] = NSOpenGLPFASamples;
    attributes[numAttributes++] = sampleCount;
  } else {
    attributes[numAttributes++] = NSOpenGLPFASampleBuffers;
    attributes[numAttributes++] = 0;
  }
  attributes[numAttributes++] = 0;
  assert(numAttributes <= kMaxAttributes);
  return [[NSOpenGLPixelFormat alloc] initWithAttributes:attributes];
}
}  // namespace

class BenchGLContextMac : public BenchGLContext {
 public:
  BenchGLContextMac();

  bool MakeCurrent() override;

  bool ClearCurrent() override;

  bool IsCurrent() override;

 private:
  NSOpenGLContext *context_;
};

BenchGLContextMac::BenchGLContextMac() {
  NSOpenGLPixelFormat *pixelFormat = GetGLPixelFormat(1);
  context_ = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
}

using SkityGLFuncPtr = void (*)();
SkityGLFuncPtr GetProcLoader(const char *procname) {
  static CFBundleRef esBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.opengl"));
  CFStringRef symbolName =
      CFStringCreateWithCString(kCFAllocatorDefault, procname, kCFStringEncodingASCII);
  SkityGLFuncPtr symbol = (SkityGLFuncPtr)CFBundleGetFunctionPointerForName(esBundle, symbolName);
  CFRelease(symbolName);
  return symbol;
}

bool BenchGLContextMac::MakeCurrent() {
  if (!context_) {
    return false;
  }
  [context_ makeCurrentContext];
  return true;
}
bool BenchGLContextMac::ClearCurrent() {
  [NSOpenGLContext clearCurrentContext];
  return true;
}

bool BenchGLContextMac::IsCurrent() { return [NSOpenGLContext currentContext] == context_; }
void *GetGLProcLoader() { return (void *)GetProcLoader; }

std::unique_ptr<BenchGLContext> CreateBenchGLContextMac() {
  return std::make_unique<BenchGLContextMac>();
}

}  // namespace skity
