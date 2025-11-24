// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/gpu/web/gpu_command_buffer_web.hpp"

#include "src/gpu/web/gpu_blit_pass_web.hpp"
#include "src/gpu/web/gpu_render_pass_web.hpp"
#include "src/logging.hpp"

namespace skity {

namespace {

struct PendingData {
  PendingData(std::vector<WGPUBuffer> stage_buffers,
              std::vector<WGPUBindGroup> bind_groups)
      : stage_buffers(std::move(stage_buffers)),
        bind_groups(std::move(bind_groups)) {}

  ~PendingData() {
    for (auto buffer : stage_buffers) {
      wgpuBufferDestroy(buffer);
      wgpuBufferRelease(buffer);
    }

    for (auto bind_group : bind_groups) {
      wgpuBindGroupRelease(bind_group);
    }
  }

  std::vector<WGPUBuffer> stage_buffers;
  std::vector<WGPUBindGroup> bind_groups;
};

void OnQueueSubmittedWorkDone(WGPUQueueWorkDoneStatus status,
                              WGPUStringView message, void* userdata1,
                              void* userdata2) {
  LOGI("OnQueueSubmittedWorkDone: {} ",
       std::string(message.data, message.length));

  auto data = reinterpret_cast<PendingData*>(userdata1);

  delete data;
}

}  // namespace

GPUCommandBufferWEB::GPUCommandBufferWEB(WGPUDevice device, WGPUQueue queue,
                                         WGPUCommandEncoder encoder)
    : device_(device), queue_(queue), encoder_(encoder), stage_buffers_() {}

GPUCommandBufferWEB::~GPUCommandBufferWEB() {
  wgpuCommandEncoderRelease(encoder_);
}

std::shared_ptr<GPUBlitPass> GPUCommandBufferWEB::BeginBlitPass() {
  return std::make_shared<GPUBlitPassWEB>(device_, encoder_, this);
}

std::shared_ptr<GPURenderPass> GPUCommandBufferWEB::BeginRenderPass(
    const GPURenderPassDescriptor& desc) {
  return std::make_shared<GPURenderPassWEB>(desc, this, device_, encoder_);
}

void GPUCommandBufferWEB::RecordStageBuffer(WGPUBuffer buffer) {
  stage_buffers_.emplace_back(buffer);
}

void GPUCommandBufferWEB::RecordBindGroup(WGPUBindGroup bind_group) {
  bind_groups_.emplace_back(bind_group);
}

bool GPUCommandBufferWEB::Submit() {
  WGPUCommandBufferDescriptor desc = {};

  WGPUCommandBuffer command_buffer = wgpuCommandEncoderFinish(encoder_, &desc);

  if (!command_buffer) {
    return false;
  }

  wgpuQueueSubmit(queue_, 1, &command_buffer);

  wgpuQueueOnSubmittedWorkDone(
      queue_, WGPUQueueWorkDoneCallbackInfo{
                  .nextInChain = nullptr,
                  .mode = WGPUCallbackMode_AllowSpontaneous,
                  .callback = OnQueueSubmittedWorkDone,
                  .userdata1 = new PendingData(std::move(stage_buffers_),
                                               std::move(bind_groups_)),
                  .userdata2 = nullptr,
              });

  return true;
}

}  // namespace skity
