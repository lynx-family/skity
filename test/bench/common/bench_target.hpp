// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef TEST_BENCH_COMMON_BENCH_TARGET_HPP
#define TEST_BENCH_COMMON_BENCH_TARGET_HPP

#include <_types/_uint8_t.h>

#include <skity/gpu/gpu_backend_type.hpp>
#include <skity/gpu/gpu_context.hpp>
#include <skity/skity.hpp>

namespace skity {

class BenchTarget {
 public:
  enum AAType : uint8_t {
    kNoAA = 0,
    kMSAA = 1,
    kContourAA = 2,
  };

  struct Options {
    uint32_t width;
    uint32_t height;
    AAType aa = AAType::kNoAA;
  };

  BenchTarget(skity::GPUContext *context,
              std::unique_ptr<skity::GPUSurface> surface, Options options)
      : width_(options.width),
        height_(options.height),
        context_(context),
        surface_(std::move(surface)) {}

  virtual ~BenchTarget() = default;

  Canvas *LockCanvas();

  void Flush();

  int32_t GetWidth() const { return width_; }
  int32_t GetHeight() const { return height_; }

 private:
  uint32_t width_;
  uint32_t height_;
  skity::GPUContext *context_;
  std::unique_ptr<skity::GPUSurface> surface_;
  Canvas *canvas_;
};

}  // namespace skity

#endif  // TEST_BENCH_COMMON_BENCH_TARGET_HPP
