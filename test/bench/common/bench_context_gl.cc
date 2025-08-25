// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "test/bench/common/bench_context_gl.hpp"

#include <memory>
#include <skity/codec/codec.hpp>
#include <skity/gpu/gpu_context.hpp>
#include <skity/gpu/gpu_context_gl.hpp>
#include <skity/graphic/alpha_type.hpp>
#include <skity/graphic/image.hpp>

#include "test/bench/common/bench_color_utils.hpp"
#include "test/bench/common/bench_context.hpp"
#include "test/bench/common/bench_gl_context.hpp"
#include "test/bench/common/bench_gl_context_mac.h"
#include "test/bench/common/bench_target.hpp"
#include "test/bench/common/bench_target_gl.hpp"

namespace skity {

namespace {
void FlipY(uint8_t *addr, int width, int height) {
  auto row_bytes = width * 4;
  auto buffer = new uint8_t[row_bytes];
  for (int y = 0; y < height / 2; ++y) {
    auto top = addr + y * row_bytes;
    auto bottom = addr + (height - y - 1) * row_bytes;
    memcpy(buffer, top, row_bytes);
    memcpy(top, bottom, row_bytes);
    memcpy(bottom, buffer, row_bytes);
  }
}
}  // namespace

class BenchContextGL : public BenchContext {
 public:
  BenchContextGL(std::unique_ptr<skity::GPUContext> gpu_context,
                 std::unique_ptr<BenchGLContext> gl_context)
      : BenchContext(std::move(gpu_context)),
        gl_context_(std::move(gl_context)) {}

  ~BenchContextGL() override { gl_context_->ClearCurrent(); }

  std::shared_ptr<BenchTarget> CreateTarget(
      BenchTarget::Options options) override;

  bool WriteToFile(std::shared_ptr<BenchTarget> target,
                   std::string path) override;

 private:
  std::unique_ptr<BenchGLContext> gl_context_;
};

std::shared_ptr<BenchTarget> BenchContextGL::CreateTarget(
    BenchTarget::Options options) {
  return BenchTargetGL::Create(gpu_context_.get(), options);
}
bool BenchContextGL::WriteToFile(std::shared_ptr<BenchTarget> target,
                                 std::string path) {
  auto target_gl_skity = static_cast<BenchTargetGL *>(target.get());
  auto gl_texture = target_gl_skity->GetTexture();
  skity::GPUBackendTextureInfoGL backend_texture_info;
  backend_texture_info.tex_id = gl_texture;
  backend_texture_info.backend = skity::GPUBackendType::kOpenGL;
  backend_texture_info.width = target_gl_skity->GetWidth();
  backend_texture_info.height = target_gl_skity->GetHeight();
  auto skity_texture =
      gpu_context_->WrapTexture(&backend_texture_info, nullptr, nullptr);
  auto image = skity::Image::MakeHWImage(skity_texture);
  auto pixmap = image->ReadPixels(gpu_context_.get());
  uint8_t *addr = reinterpret_cast<uint8_t *>(pixmap->WritableAddr());
  UnpremultiplyAlpha(addr, pixmap->Width() * pixmap->Height());
  FlipY(addr, pixmap->Width(), pixmap->Height());

  auto codec = skity::Codec::MakePngCodec();

  auto encoded_data = codec->Encode(pixmap.get());

  char full_file_name[128];

  snprintf(full_file_name, sizeof(full_file_name), "%s.png", path.c_str());

  encoded_data->WriteToFile(full_file_name);
  return true;
}

std::shared_ptr<BenchContext> CreateBenchContextGL(void *proc_loader) {
  auto gl_context = CreateBenchGLContextMac();
  gl_context->MakeCurrent();
  auto gpu_context = skity::GLContextCreate(proc_loader);
  return std::make_shared<BenchContextGL>(std::move(gpu_context),
                                          std::move(gl_context));
}

}  // namespace skity
