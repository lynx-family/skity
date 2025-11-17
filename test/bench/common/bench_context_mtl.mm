// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/common/bench_context_mtl.h"

#include <skity/gpu/gpu_context_mtl.h>
#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/skity.hpp>

#include "test/bench/common/bench_color_utils.hpp"
#include "test/bench/common/bench_context.hpp"
#include "test/bench/common/bench_target.hpp"
#include "test/bench/common/bench_target_mtl.h"

#import "test/bench/common/bench_command_queue_proxy.h"

namespace skity {

class BenchContextMTL : public BenchContext {
 public:
  BenchContextMTL(std::unique_ptr<skity::GPUContext> gpu_context, id<MTLDevice> device,
                  id<MTLCommandQueue> queue);

  std::shared_ptr<BenchTarget> CreateTarget(BenchTarget::Options options) override;

  bool WriteToFile(std::shared_ptr<BenchTarget> target, std::string path) override;

 private:
  id<MTLDevice> device_;
  id<MTLCommandQueue> queue_;
};

std::shared_ptr<BenchContext> CreateBenchContextMTL() {
  auto device = MTLCreateSystemDefaultDevice();
  auto queue = [device newCommandQueue];
  id<MTLCommandQueue> proxy_queue = [[CommandQueueProxy alloc] initWithRealCommandQueue:queue];
  auto gpu_context = skity::MTLContextCreate(device, proxy_queue);
  return std::make_shared<BenchContextMTL>(std::move(gpu_context), device, queue);
}

BenchContextMTL::BenchContextMTL(std::unique_ptr<skity::GPUContext> gpu_context,
                                 id<MTLDevice> device, id<MTLCommandQueue> queue)
    : BenchContext(std::move(gpu_context)), device_(device), queue_(queue) {}

std::shared_ptr<BenchTarget> BenchContextMTL::CreateTarget(BenchTarget::Options options) {
  return BenchTargetMTL::Create(gpu_context_.get(), options);
}

bool BenchContextMTL::WriteToFile(std::shared_ptr<BenchTarget> target, std::string path) {
  auto *mtl_skity_target = static_cast<BenchTargetMTL *>(target.get());

  id<MTLBuffer> buffer = [device_ newBufferWithLength:target->GetWidth() * target->GetHeight() * 4
                                              options:MTLResourceStorageModeShared];
  id<MTLCommandBuffer> command_buffer = [queue_ commandBuffer];
  id<MTLBlitCommandEncoder> blit_command_encoder = [command_buffer blitCommandEncoder];
  auto dst_bytes_per_row = target->GetWidth() * 4;
  auto dst_bytes_per_image = target->GetHeight() * dst_bytes_per_row;
  [blit_command_encoder copyFromTexture:mtl_skity_target->GetTexture()
                            sourceSlice:0
                            sourceLevel:0
                           sourceOrigin:MTLOriginMake(0, 0, 0)
                             sourceSize:MTLSizeMake(target->GetWidth(), target->GetHeight(), 1)
                               toBuffer:buffer
                      destinationOffset:0
                 destinationBytesPerRow:dst_bytes_per_row
               destinationBytesPerImage:dst_bytes_per_image];
  [blit_command_encoder endEncoding];
  [command_buffer commit];
  [command_buffer waitUntilCompleted];
  auto data = skity::Data::MakeWithCopy(buffer.contents, buffer.length);

  uint8_t *addr = reinterpret_cast<uint8_t *>(const_cast<void *>(data->RawData()));
  UnpremultiplyAlpha(addr, data->Size() / 4);

  auto pixmap = skity::Pixmap(data, target->GetWidth(), target->GetHeight(),
                              skity::AlphaType::kPremul_AlphaType,
                              skity::ColorType::kBGRA);  // TODO color type and alpha type

  auto codec = skity::Codec::MakePngCodec();

  auto encoded_data = codec->Encode(&pixmap);

  char full_file_name[128];

  snprintf(full_file_name, sizeof(full_file_name), "%s.png", path.c_str());

  encoded_data->WriteToFile(full_file_name);

  return true;
}

}  // namespace skity
