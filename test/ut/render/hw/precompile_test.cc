// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <memory>
#include <skity/effect/color_filter.hpp>
#include <skity/effect/shader.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/io/pixmap.hpp>
#include <skity/render/canvas.hpp>
#include <skity/render/precompile_context.hpp>
#include <skity/text/text_blob.hpp>
#include <string>
#include <utility>
#include <vector>

#include "src/gpu/gpu_blit_pass.hpp"
#include "src/gpu/gpu_buffer.hpp"
#include "src/gpu/gpu_command_buffer.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/gpu/gpu_device.hpp"
#include "src/gpu/gpu_render_pipeline.hpp"
#include "src/gpu/gpu_sampler.hpp"
#include "src/gpu/gpu_shader_function.hpp"
#include "src/gpu/gpu_shader_module.hpp"
#include "src/gpu/gpu_surface_impl.hpp"
#include "src/gpu/gpu_texture.hpp"
#include "src/render/hw/draw/hw_dynamic_coverage_path_draw.hpp"
#include "src/render/hw/layer/hw_root_layer.hpp"

namespace skity {
namespace {

class FakeShaderFunction : public GPUShaderFunction {
 public:
  explicit FakeShaderFunction(GPULabel label)
      : GPUShaderFunction(std::move(label)) {}

  bool IsValid() const override { return true; }
};

class FakeRenderPipeline : public GPURenderPipeline {
 public:
  explicit FakeRenderPipeline(const GPURenderPipelineDescriptor& desc)
      : GPURenderPipeline(desc) {}
};

class FakeSampler : public GPUSampler {
 public:
  explicit FakeSampler(const GPUSamplerDescriptor& desc) : GPUSampler(desc) {}

  ~FakeSampler() override = default;
};

class FakeGPUTexture : public GPUTexture {
 public:
  explicit FakeGPUTexture(const GPUTextureDescriptor& desc)
      : GPUTexture(desc) {}

  ~FakeGPUTexture() override = default;

  size_t GetBytes() const override {
    return desc_.width * desc_.height *
           GetTextureFormatBytesPerPixel(desc_.format);
  }

  void UploadData(uint32_t, uint32_t, uint32_t, uint32_t, void*) override {}
};

class FakeBlitPass : public GPUBlitPass {
 public:
  void UploadTextureData(std::shared_ptr<GPUTexture>, uint32_t, uint32_t,
                         uint32_t, uint32_t, void*) override {}

  void UploadBufferData(GPUBuffer*, void*, size_t) override {}

  void GenerateMipmaps(const std::shared_ptr<GPUTexture>&) override {}

  void End() override {}
};

class FakeRenderPass : public GPURenderPass {
 public:
  FakeRenderPass(const GPURenderPassDescriptor& desc,
                 std::vector<uint32_t>& instance_counts)
      : GPURenderPass(desc), instance_counts_(instance_counts) {}

  void EncodeCommands(std::optional<GPUViewport> = std::nullopt,
                      std::optional<GPUScissorRect> = std::nullopt) override {
    for (const auto* command : GetCommands()) {
      instance_counts_.push_back(command->instance_count);
    }
  }

 private:
  std::vector<uint32_t>& instance_counts_;
};

class FakeCommandBuffer : public GPUCommandBuffer {
 public:
  std::shared_ptr<GPURenderPass> BeginRenderPass(
      const GPURenderPassDescriptor& desc) override {
    render_pass_count_++;
    return std::make_shared<FakeRenderPass>(desc, instance_counts_);
  }

  std::shared_ptr<GPUBlitPass> BeginBlitPass() override {
    return std::make_shared<FakeBlitPass>();
  }

  bool Submit(const GPUSubmitInfo* = nullptr) override { return true; }

  uint32_t render_pass_count() const { return render_pass_count_; }

  const std::vector<uint32_t>& instance_counts() const {
    return instance_counts_;
  }

 private:
  uint32_t render_pass_count_ = 0;
  std::vector<uint32_t> instance_counts_;
};

class FakeGPUDevice : public GPUDevice {
 public:
  using GPUDevice::InitCaps;

  FakeGPUDevice() {
    auto caps = std::make_unique<GPUCaps>();
    InitCaps(std::move(caps));
  }

  std::unique_ptr<GPUBuffer> CreateBuffer(
      const GPUBufferDescriptor& desc) override {
    return std::make_unique<GPUBuffer>(desc);
  }

  std::shared_ptr<GPUShaderFunction> CreateShaderFunction(
      const GPUShaderFunctionDescriptor& desc) override {
    shader_function_count_++;
    if (disallow_shader_pipeline_creation_) {
      disallowed_shader_function_count_++;
    }

    auto function = std::make_shared<FakeShaderFunction>(desc.label);
    if (desc.source_type == GPUShaderSourceType::kWGX &&
        desc.shader_source != nullptr) {
      auto source = static_cast<GPUShaderSourceWGX*>(desc.shader_source);
      function->SetWGXContext(source->context);
    }
    return function;
  }

  std::unique_ptr<GPURenderPipeline> CreateRenderPipeline(
      const GPURenderPipelineDescriptor& desc) override {
    render_pipeline_count_++;
    if (disallow_shader_pipeline_creation_) {
      disallowed_render_pipeline_count_++;
    }
    if (fail_render_pipeline_creation_) {
      return nullptr;
    }
    if (desc.fragment_function != nullptr) {
      fragment_function_labels_.push_back(desc.fragment_function->GetLabel());
    }
    if (desc.vertex_function != nullptr) {
      vertex_function_labels_.push_back(desc.vertex_function->GetLabel());
    }
    return std::make_unique<FakeRenderPipeline>(desc);
  }

  std::unique_ptr<GPURenderPipeline> ClonePipeline(
      GPURenderPipeline*, const GPURenderPipelineDescriptor& desc) override {
    clone_pipeline_count_++;
    if (disallow_shader_pipeline_creation_) {
      disallowed_clone_pipeline_count_++;
    }
    return std::make_unique<FakeRenderPipeline>(desc);
  }

  std::shared_ptr<GPUCommandBuffer> CreateCommandBuffer() override {
    last_command_buffer_ = std::make_shared<FakeCommandBuffer>();
    return last_command_buffer_;
  }

  std::shared_ptr<GPUSampler> CreateSampler(
      const GPUSamplerDescriptor& desc) override {
    sampler_count_++;
    return std::make_shared<FakeSampler>(desc);
  }

  std::shared_ptr<GPUTexture> CreateTexture(
      const GPUTextureDescriptor& desc) override {
    texture_count_++;
    auto texture_binding =
        static_cast<GPUTextureUsageMask>(GPUTextureUsage::kTextureBinding);
    if (desc.format == GPUTextureFormat::kRGBA16Uint &&
        desc.storage_mode == GPUTextureStorageMode::kHostVisible &&
        (desc.usage & texture_binding) != 0) {
      coverage_aa_line_texture_count_++;
    }
    if (fail_texture_creation_) {
      return nullptr;
    }
    return std::make_shared<FakeGPUTexture>(desc);
  }

  bool CanUseMSAA() override { return true; }

  uint32_t GetBufferAlignment() override { return 256; }

  uint32_t GetMaxTextureSize() override { return 4096; }

  uint32_t shader_function_count() const { return shader_function_count_; }

  uint32_t render_pipeline_count() const { return render_pipeline_count_; }

  uint32_t clone_pipeline_count() const { return clone_pipeline_count_; }

  uint32_t texture_count() const { return texture_count_; }

  uint32_t coverage_aa_line_texture_count() const {
    return coverage_aa_line_texture_count_;
  }

  uint32_t sampler_count() const { return sampler_count_; }

  uint32_t last_render_pass_count() const {
    return last_command_buffer_ == nullptr
               ? 0u
               : last_command_buffer_->render_pass_count();
  }

  std::vector<uint32_t> last_instance_counts() const {
    return last_command_buffer_ == nullptr
               ? std::vector<uint32_t>{}
               : last_command_buffer_->instance_counts();
  }

  uint32_t disallowed_shader_function_count() const {
    return disallowed_shader_function_count_;
  }

  uint32_t disallowed_render_pipeline_count() const {
    return disallowed_render_pipeline_count_;
  }

  uint32_t disallowed_clone_pipeline_count() const {
    return disallowed_clone_pipeline_count_;
  }

  bool HasFragmentFunctionLabelContaining(const std::string& text) const {
    for (const auto& label : fragment_function_labels_) {
      if (label.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  bool HasFragmentFunctionLabel(const std::string& text) const {
    for (const auto& label : fragment_function_labels_) {
      if (label == text) {
        return true;
      }
    }
    return false;
  }

  bool HasVertexFunctionLabelContaining(const std::string& text) const {
    for (const auto& label : vertex_function_labels_) {
      if (label.find(text) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  void set_fail_render_pipeline_creation(bool fail) {
    fail_render_pipeline_creation_ = fail;
  }

  void set_fail_texture_creation(bool fail) { fail_texture_creation_ = fail; }

  void set_disallow_shader_pipeline_creation(bool disallow) {
    disallow_shader_pipeline_creation_ = disallow;
    disallowed_shader_function_count_ = 0;
    disallowed_render_pipeline_count_ = 0;
    disallowed_clone_pipeline_count_ = 0;
  }

 private:
  uint32_t shader_function_count_ = 0;
  uint32_t render_pipeline_count_ = 0;
  uint32_t clone_pipeline_count_ = 0;
  uint32_t disallowed_shader_function_count_ = 0;
  uint32_t disallowed_render_pipeline_count_ = 0;
  uint32_t disallowed_clone_pipeline_count_ = 0;
  uint32_t texture_count_ = 0;
  uint32_t coverage_aa_line_texture_count_ = 0;
  uint32_t sampler_count_ = 0;
  std::vector<std::string> fragment_function_labels_;
  std::vector<std::string> vertex_function_labels_;
  std::shared_ptr<FakeCommandBuffer> last_command_buffer_;
  bool disallow_shader_pipeline_creation_ = false;
  bool fail_render_pipeline_creation_ = false;
  bool fail_texture_creation_ = false;
};

class FakeRootLayer : public HWRootLayer {
 public:
  FakeRootLayer(uint32_t width, uint32_t height, const Rect& bounds,
                GPUTextureFormat format, HWDrawState* last_draw_state,
                bool supports_texture_copy_dst_read)
      : HWRootLayer(width, height, bounds, format),
        last_draw_state_(last_draw_state),
        supports_texture_copy_dst_read_(supports_texture_copy_dst_read) {}

  bool SupportsTextureCopyDstRead() const override {
    return supports_texture_copy_dst_read_;
  }

 private:
  std::shared_ptr<GPURenderPass> OnBeginRenderPass(GPUCommandBuffer* cmd,
                                                   bool force_load) override {
    if (last_draw_state_ != nullptr) {
      *last_draw_state_ = GetLayerDrawState();
    }

    GPUTextureDescriptor texture_desc{};
    texture_desc.width = GetWidth();
    texture_desc.height = GetHeight();
    texture_desc.format = GetColorFormat();

    auto texture = std::make_shared<FakeGPUTexture>(texture_desc);

    GPURenderPassDescriptor desc{};
    desc.color_attachment.texture = texture;
    desc.stencil_attachment.texture = texture;
    desc.depth_attachment.texture = texture;
    desc.color_attachment.load_op = (force_load || !NeedClearSurface())
                                        ? GPULoadOp::kLoad
                                        : GPULoadOp::kClear;
    desc.stencil_attachment.load_op = GPULoadOp::kClear;
    desc.depth_attachment.load_op = GPULoadOp::kClear;
    desc.label = "FakeRootLayer";
    return cmd->BeginRenderPass(desc);
  }

  void OnPostDraw(GPURenderPass*, GPUCommandBuffer*) override {}

  bool OnCopyToDstTexture(GPUCommandBuffer*, std::shared_ptr<GPUTexture>,
                          GPURegion) const override {
    return true;
  }

 private:
  HWDrawState* last_draw_state_ = nullptr;
  bool supports_texture_copy_dst_read_ = true;
};

class FakeGPUSurface : public GPUSurfaceImpl {
 public:
  FakeGPUSurface(const GPUSurfaceDescriptor& desc, GPUContextImpl* ctx,
                 GPUTextureFormat format)
      : GPUSurfaceImpl(desc, ctx), format_(format) {}

  GPUTextureFormat GetGPUFormat() const override { return format_; }

  std::shared_ptr<Pixmap> ReadPixels(const Rect&) override { return nullptr; }

  HWDrawState last_draw_state() const { return last_draw_state_; }

  void SetSupportsTextureCopyDstRead(bool supports) {
    supports_texture_copy_dst_read_ = supports;
  }

 protected:
  HWRootLayer* OnBeginNextFrame(bool clear) override {
    auto* root_layer = GetArenaAllocator()->Make<FakeRootLayer>(
        GetWidth(), GetHeight(), Rect::MakeWH(GetWidth(), GetHeight()),
        GetGPUFormat(), &last_draw_state_, supports_texture_copy_dst_read_);
    root_layer->SetClearSurface(clear);
    root_layer->SetSampleCount(GetSampleCount());
    root_layer->SetArenaAllocator(GetArenaAllocator());
    return root_layer;
  }

  void OnFlush() override {}

 private:
  GPUTextureFormat format_;
  HWDrawState last_draw_state_ = HWDrawState::kDrawStateNone;
  bool supports_texture_copy_dst_read_ = true;
};

class FakeGPUContext : public GPUContextImpl {
 public:
  FakeGPUContext() : GPUContextImpl(GPUBackendType::kNone) {}

  FakeGPUDevice* device() const {
    return static_cast<FakeGPUDevice*>(GetGPUDevice());
  }

  std::unique_ptr<GPUSurface> CreateSurface(
      GPUSurfaceDescriptor* desc) override {
    return std::make_unique<FakeGPUSurface>(*desc, this,
                                            GPUTextureFormat::kRGBA8Unorm);
  }

 protected:
  std::unique_ptr<GPUDevice> CreateGPUDevice() override {
    return std::make_unique<FakeGPUDevice>();
  }

  std::shared_ptr<GPUTexture> OnWrapTexture(GPUBackendTextureInfo*,
                                            ReleaseCallback,
                                            ReleaseUserData) override {
    return nullptr;
  }

  std::unique_ptr<GPURenderTarget> OnCreateRenderTarget(
      const GPURenderTargetDescriptor& desc,
      std::shared_ptr<Texture> texture) override {
    GPUSurfaceDescriptor surface_desc{};
    surface_desc.width = desc.width;
    surface_desc.height = desc.height;
    surface_desc.sample_count = desc.sample_count;
    surface_desc.render_options = desc.render_options;
    auto surface = std::make_unique<FakeGPUSurface>(
        surface_desc, this, GPUTextureFormat::kRGBA8Unorm);
    return std::make_unique<GPURenderTarget>(std::move(surface),
                                             std::move(texture));
  }

  std::shared_ptr<Data> OnReadPixels(
      const std::shared_ptr<GPUTexture>&) const override {
    return nullptr;
  }
};

std::unique_ptr<PrecompileContext> MakePrecompileContext(
    FakeGPUContext& context, bool enable_msaa) {
  return context.CreatePrecompileContext(PrecompileColorType::kRGBA,
                                         enable_msaa);
}

void PrecompileDefaultShaders(FakeGPUContext& context, bool enable_msaa) {
  MakePrecompileContext(context, enable_msaa)->PrecompileDefaultShaders();
}

void PrecompileDraw(FakeGPUContext& context, bool enable_msaa,
                    PrecompileDrawType draw_type, const Paint& paint) {
  MakePrecompileContext(context, enable_msaa)->PrecompileDraw(draw_type, paint);
}

Path MakeTestPath() {
  Path path;
  path.MoveTo(0.f, 0.f);
  path.LineTo(16.f, 0.f);
  path.LineTo(0.f, 16.f);
  path.LineTo(16.f, 16.f);
  path.Close();
  return path;
}

Path MakeConvexTestPath() {
  Path path;
  path.MoveTo(2.f, 2.f);
  path.LineTo(24.f, 4.f);
  path.LineTo(8.f, 26.f);
  path.Close();
  return path;
}

std::shared_ptr<Image> MakeTestImage() {
  auto pixmap = std::make_shared<Pixmap>(1, 1, AlphaType::kPremul_AlphaType,
                                         ColorType::kRGBA);
  return Image::MakeImage(std::move(pixmap));
}

Paint MakeImagePaint(const std::shared_ptr<Image>& image) {
  Paint paint;
  paint.SetShader(Shader::MakeShader(image, SamplingOptions{}, TileMode::kDecal,
                                     TileMode::kDecal, Matrix{}));
  return paint;
}

template <typename DrawProc>
void ExpectRealDrawHitsPrecompiledPipeline(FakeGPUContext& context,
                                           PrecompileDrawType draw_type,
                                           const Paint& paint, bool enable_msaa,
                                           DrawProc draw_proc) {
  auto precompile_context = MakePrecompileContext(context, enable_msaa);
  precompile_context->PrecompileDraw(draw_type, paint);

  auto* device = context.device();
  ASSERT_NE(device, nullptr);
  device->set_disallow_shader_pipeline_creation(true);

  GPUSurfaceDescriptor desc{};
  desc.width = 32;
  desc.height = 32;
  desc.sample_count = enable_msaa ? 4 : 1;
  auto surface = context.CreateSurface(&desc);
  auto* canvas = surface->LockCanvas();
  draw_proc(canvas);
  canvas->Flush();
  surface->Flush();

  EXPECT_EQ(device->disallowed_shader_function_count(), 0u);
  EXPECT_EQ(device->disallowed_render_pipeline_count(), 0u);
  EXPECT_EQ(device->disallowed_clone_pipeline_count(), 0u);

  device->set_disallow_shader_pipeline_creation(false);
}

}  // namespace

TEST(PrecompileDrawTest, ReturnsBeforeContextInit) {
  FakeGPUContext context;
  Paint paint;

  PrecompileDefaultShaders(context, false);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);
}

TEST(PrecompileDrawTest, InvalidPrecompileColorTypeDoesNotCreateGPUResources) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  EXPECT_FALSE(context.IsForceDepthStencilPipelineState());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  auto invalid_context = context.CreatePrecompileContext(
      static_cast<PrecompileColorType>(255), false);

  Paint paint;
  invalid_context->PrecompileDefaultShaders();
  invalid_context->PrecompileDraw(PrecompileDrawType::kDrawImage, paint);

  EXPECT_FALSE(context.IsForceDepthStencilPipelineState());
  EXPECT_EQ(device->shader_function_count(), 0u);
  EXPECT_EQ(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, PrecompileContextForcesDepthStencilLayerState) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  EXPECT_FALSE(context.IsForceDepthStencilPipelineState());

  auto precompile_context = MakePrecompileContext(context, false);
  ASSERT_NE(precompile_context, nullptr);
  EXPECT_TRUE(context.IsForceDepthStencilPipelineState());

  GPUSurfaceDescriptor desc{};
  desc.width = 32;
  desc.height = 32;
  auto surface = context.CreateSurface(&desc);
  auto* fake_surface = static_cast<FakeGPUSurface*>(surface.get());

  Paint paint;
  auto* canvas = surface->LockCanvas();
  canvas->DrawRect(Rect::MakeWH(16.f, 16.f), paint);
  canvas->Flush();
  surface->Flush();

  EXPECT_EQ(fake_surface->last_draw_state(),
            kDrawStateDepth | kDrawStateStencil);
}

TEST(PrecompileDrawTest, PrecompileDefaultShadersSupportsCommonDraws) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  PrecompileDefaultShaders(context, false);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDefaultShaders(context, false);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompileDefaultShadersCoversExplicitDefaultDraws) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);

  PrecompileDefaultShaders(context, false);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImageRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompileDefaultShadersCoversGPUTessellationPath) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  PrecompileDefaultShaders(context, false);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
}

TEST(PrecompileDrawTest,
     PrecompileDefaultShadersCoversTwoColorLinearGradientPath) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  PrecompileDefaultShaders(context, false);

  EXPECT_TRUE(device->HasFragmentFunctionLabel(
      "FS_GradientLinear2OffsetFastColorFast"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel("FS_GradientLinear2ColorFast"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel("FS_GradientLinear2"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  Paint paint;
  Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {32.f, 32.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, nullptr, 2));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  float color_fast_pos[] = {0.f, 1.f};
  paint.SetShader(Shader::MakeLinear(pts, colors, color_fast_pos, 2));
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  float pos[] = {0.25f, 0.75f};
  paint.SetShader(Shader::MakeLinear(pts, colors, pos, 2));
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompilePathUsesContourAAWhenEnabled) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetAntiAlias(true);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("PathAA"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
}

TEST(PrecompileDrawTest, AnalyticalAAModesCanBeEnabledTogether) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());

  context.SetEnableContourAA(true);
  EXPECT_TRUE(context.IsEnableContourAA());
  EXPECT_FALSE(context.IsEnableCoverageAA());

  context.SetEnableCoverageAA(true);
  EXPECT_TRUE(context.IsEnableCoverageAA());
  EXPECT_TRUE(context.IsEnableContourAA());
}

TEST(PrecompileDrawTest, AnalyticalAAModesAreMutuallyExclusiveInsideCanvas) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.0f;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Matrix perspective;
  perspective.SetPersp0(0.001f);
  canvas->Concat(perspective);

  Path path;
  path.MoveTo(8, 8);
  path.LineTo(56, 8);
  path.LineTo(8, 56);
  path.Close();

  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPath"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 0u);
}

TEST(PrecompileDrawTest,
     SurfaceAnalyticalModeOverridesContextConflationCorrection) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetConflationCorrection(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.0f;
  surface_desc.render_options.coverage_aa = CoverageAAMode::kAnalytical;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.MoveTo(16, 16);
  path.LineTo(48, 16);
  path.LineTo(48, 48);
  path.LineTo(16, 48);
  path.Close();
  path.SetFillType(Path::PathFillType::kEvenOdd);

  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);

  Path second_path;
  second_path.MoveTo(8, 8);
  second_path.LineTo(24, 8);
  second_path.LineTo(8, 24);
  second_path.Close();
  second_path.SetFillType(Path::PathFillType::kEvenOdd);
  canvas->DrawPath(second_path, paint);

  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAAFill"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("CoverageAAFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining(
      "CoverageAAConflationCorrection"));
  EXPECT_EQ(device->last_render_pass_count(), 1u);
}

TEST(PrecompileDrawTest, SurfaceCanDisableCoverageAADefault) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.render_options.coverage_aa = CoverageAAMode::kDisabled;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 0u);
}

TEST(PrecompileDrawTest, SurfaceCanEnableCoverageAAWithoutContextDefault) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.render_options.coverage_aa = CoverageAAMode::kAnalytical;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 1u);
}

TEST(PrecompileDrawTest, FallsBackWhenFragmentMaskBlendIsUnsupported) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  auto surface = context.CreateSurface(&surface_desc);
  static_cast<FakeGPUSurface*>(surface.get())
      ->SetSupportsTextureCopyDstRead(false);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Paint paint;
  paint.SetAntiAlias(true);
  paint.SetColor(0x80FFFFFF);
  paint.SetBlendMode(BlendMode::kSrc);
  canvas->DrawRRect(RRect::MakeRectXY(Rect::MakeLTRB(8, 8, 56, 56), 8.f, 8.f),
                    paint);
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("Path"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 0u);
}

TEST(PrecompileDrawTest, SurfaceCanSelectConflationCorrection) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 64;
  surface_desc.height = 64;
  surface_desc.sample_count = 1;
  surface_desc.render_options.coverage_aa =
      CoverageAAMode::kConflationCorrection;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);
  canvas->Flush();

  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining(
      "CoverageAAConflationCorrection"));
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 1u);
}

TEST(PrecompileDrawTest, RenderTargetUsesSurfaceCoverageAAOptions) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  context.SetConflationCorrection(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPURenderTargetDescriptor target_desc{};
  target_desc.width = 64;
  target_desc.height = 64;
  target_desc.render_options.coverage_aa = CoverageAAMode::kAnalytical;
  auto target = context.CreateRenderTarget(target_desc);
  ASSERT_NE(target, nullptr);

  Path path;
  path.AddRect(Rect::MakeLTRB(8, 8, 56, 56));
  Paint paint;
  paint.SetAntiAlias(true);
  target->GetCanvas()->DrawPath(path, paint);

  ASSERT_NE(context.MakeSnapshot(std::move(target)), nullptr);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining(
      "CoverageAAConflationCorrection"));
}

TEST(PrecompileDrawTest, CoverageAADoesNotAddLayerMaskPasses) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableCoverageAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  GPUSurfaceDescriptor surface_desc{};
  surface_desc.width = 96;
  surface_desc.height = 96;
  surface_desc.sample_count = 1;
  surface_desc.content_scale = 1.0f;
  auto surface = context.CreateSurface(&surface_desc);
  auto* canvas = surface->LockCanvas(true);
  ASSERT_NE(canvas, nullptr);

  Path path;
  path.MoveTo(8, 8);
  path.LineTo(40, 8);
  path.LineTo(8, 40);
  path.Close();

  Paint paint;
  paint.SetAntiAlias(true);
  canvas->DrawPath(path, paint);

  canvas->SaveLayer(Rect::MakeWH(64, 64), Paint{});
  canvas->DrawPath(path, paint);
  canvas->Restore();
  canvas->Flush();

  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("CoverageAAFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("CoverageAA"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("CoverageAA"));
  EXPECT_EQ(device->last_render_pass_count(), 2u);
  EXPECT_EQ(device->coverage_aa_line_texture_count(), 1u);
}

TEST(PrecompileDrawTest, PrecompilePathUsesGPUTessellationWithMSAA) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  paint.SetAntiAlias(true);
  PrecompileDraw(context, true, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompilePathUsesGPUTessellationWithoutContourAA) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  paint.SetAntiAlias(true);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompilePathUsesGPUTessellationWhenPaintDisablesAA) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableContourAA(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  paint.SetAntiAlias(false);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompilePathUsesPathWhenGPUTessellationDisabled) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableGPUTessellation(false);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("VS_Path"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("PathAA"));
}

TEST(PrecompileDrawTest, PrecompileRRectUsesSolidVertexColor) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("SolidVertexColor"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
}

TEST(PrecompileDrawTest, PrecompileStrokeRectUsesRRectPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStroke_Style);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRect, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("SolidVertexColor"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("TessPath"));
}

TEST(PrecompileDrawTest, PrecompileDrawIncludesColorFilterPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetColorFilter(ColorFilters::Blend(0xFF00FF00, BlendMode::kSrcATop));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("BlendSrcATopFilter"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, PrecompileDrawIncludesAdvancedBlendingPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  paint.SetBlendMode(BlendMode::kOverlay);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_GT(device->shader_function_count(), shader_function_count);
  EXPECT_GT(device->render_pipeline_count(), render_pipeline_count);

  shader_function_count = device->shader_function_count();
  render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest,
     PrecompileRRectFallsBackToPathWhenSimpleShapeDisabled) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetStyle(Paint::kStrokeAndFill_Style);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathFill"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("TessPathStroke"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("RRect"));
}

TEST(PrecompileDrawTest, CreatesAndCachesSolidDrawPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();
  auto clone_pipeline_count = device->clone_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
  EXPECT_EQ(device->clone_pipeline_count(), clone_pipeline_count);

  PrecompileDraw(context, true, PrecompileDrawType::kDrawRRect, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
  EXPECT_GT(device->clone_pipeline_count(), 0u);
}

TEST(PrecompileDrawTest, PrecompiledPathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawPath, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledConvexTessPathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  paint.SetAntiAlias(false);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawPath, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeConvexTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledConvexPathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableGPUTessellation(false);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  paint.SetAntiAlias(false);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawPath, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeConvexTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledRRectPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);

  Paint paint;
  paint.SetStyle(Paint::kFill_Style);
  auto rrect = RRect::MakeRectXY(Rect::MakeWH(24.f, 24.f), 4.f, 4.f);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawRRect, paint, false,
      [&](Canvas* canvas) { canvas->DrawRRect(rrect, paint); });
}

TEST(SimpleShapeDrawTest, AffinePathsUseRRectPipelineAndMerge) {
  Path paths[3];
  const Rect bounds = Rect::MakeLTRB(-8.f, -6.f, 8.f, 6.f);
  paths[0].AddRect(bounds);
  paths[1].AddOval(bounds);
  paths[2].AddRoundRect(bounds, 2.f, 3.f);

  // Exact 90 degrees has zero diagonal entries but is still invertible.
  Matrix quarter_turn = Matrix::Scale(0.f, 0.f);
  quarter_turn.SetSkewX(-1.f);
  quarter_turn.SetSkewY(1.f);
  const Matrix transforms[] = {
      Matrix::RotateDeg(5.f), Matrix::RotateDeg(45.f), quarter_turn,
      Matrix::Skew(0.4f, -0.2f),
      Matrix::RotateDeg(30.f) * Matrix::Scale(-2.f, 0.5f)};
  for (size_t shape = 0; shape < 3; ++shape) {
    for (size_t transform = 0; transform < 5; ++transform) {
      for (auto style : {Paint::kFill_Style, Paint::kStroke_Style}) {
        if (shape == 0 && style == Paint::kFill_Style) continue;
        SCOPED_TRACE(::testing::Message()
                     << "shape=" << shape << " transform=" << transform
                     << " style=" << static_cast<int>(style));
        FakeGPUContext context;
        ASSERT_TRUE(context.Init());
        context.SetEnableSimpleShapePipeline(true);
        context.SetEnableGPUTessellation(false);
        context.SetEnableMergingDrawCall(true);
        GPUSurfaceDescriptor desc{};
        desc.width = 128;
        desc.height = 128;
        auto surface = context.CreateSurface(&desc);
        auto* canvas = surface->LockCanvas();
        Paint paint;
        paint.SetAntiAlias(true);
        paint.SetStyle(style);
        paint.SetStrokeWidth(2.f);
        for (int i = 0; i < 2; ++i) {
          canvas->SetMatrix(
              Matrix::Translate(32.f + i * 64.f, 32.f + i * 64.f) *
              transforms[transform]);
          canvas->DrawPath(paths[shape], paint);
        }
        canvas->Flush();
        surface->Flush();
        EXPECT_TRUE(
            context.device()->HasVertexFunctionLabelContaining("RRect"));
        EXPECT_FALSE(
            context.device()->HasVertexFunctionLabelContaining("Path"));
        EXPECT_EQ(context.device()->last_instance_counts(),
                  std::vector<uint32_t>{2u});
      }
    }
  }
}

TEST(PrecompileDrawTest,
     PrecompiledOpaqueAndTranslucentRRectPipelinesAreHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);

  auto rrect = RRect::MakeRectXY(Rect::MakeWH(24.f, 24.f), 4.f, 4.f);
  for (auto color : {0xFFFFFFFF, 0x80FFFFFF}) {
    Paint paint;
    paint.SetColor(color);
    paint.SetBlendMode(BlendMode::kSrc);
    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawRRect, paint, false,
        [&](Canvas* canvas) { canvas->DrawRRect(rrect, paint); });
  }
}

TEST(PrecompileDrawTest, PrecompiledImagePathPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableGPUTessellation(false);

  auto image = MakeTestImage();
  auto paint = MakeImagePaint(image);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawImage, paint, false,
      [&](Canvas* canvas) { canvas->DrawPath(MakeConvexTestPath(), paint); });
}

TEST(PrecompileDrawTest, PrecompiledImageRRectPipelineIsHitByRealDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  context.SetEnableSimpleShapePipeline(true);

  auto image = MakeTestImage();
  auto paint = MakeImagePaint(image);
  auto rrect = RRect::MakeRectXY(Rect::MakeWH(24.f, 24.f), 4.f, 4.f);
  ExpectRealDrawHitsPrecompiledPipeline(
      context, PrecompileDrawType::kDrawImageRRect, paint, false,
      [&](Canvas* canvas) { canvas->DrawRRect(rrect, paint); });
}

TEST(PrecompileDrawTest, ClearsFailedPipelineCacheAfterPrecompileFailure) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  device->set_fail_render_pipeline_creation(true);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_EQ(device->render_pipeline_count(), 1u);

  device->set_fail_render_pipeline_creation(false);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_EQ(device->render_pipeline_count(), 2u);
}

TEST(PrecompileDrawTest, ImagePrecompileDoesNotCreateTextureResources) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  device->set_fail_texture_creation(true);

  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("Texture"));
}

TEST(PrecompileDrawTest, ImagePrecompileUsesTexturePipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);

  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("Texture"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Solid"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("Path"));
  EXPECT_FALSE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, ImageRRectPrecompileUsesTextureRRectPipeline) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImageRRect, paint);

  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_TRUE(device->HasFragmentFunctionLabelContaining("Texture"));
  EXPECT_TRUE(device->HasVertexFunctionLabelContaining("RRect"));
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsCommonSolidDrawTypes) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawRect, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawPath, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImage, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawImageRRect, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kClipPath, paint);
  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest,
     TextBlendRoutesSplitOverlapsAndHitPrecompiledPipelines) {
  struct TestCase {
    bool texture_copy;
    bool native_blend;
    bool coherent;
  };
  const TestCase cases[] = {{true, false, false},
                            {true, true, false},
                            {true, true, true},
                            {false, false, false}};
  for (bool emoji : {false, true}) {
    auto typeface = Typeface::MakeFromFile(
        emoji ? SKITY_FONT_DIR "fonts/resources/NotoColorEmoji.ttf"
              : SKITY_FONT_DIR "fonts/resources/Roboto-Regular.ttf");
    ASSERT_NE(typeface, nullptr);
    const GlyphID glyph = typeface->UnicharToGlyph(emoji ? 0x1F600 : 'O');
    ASSERT_NE(glyph, 0);
    std::vector<TextRun> runs;
    // Identical positions guarantee overlap with either CT or FT glyph metrics.
    runs.emplace_back(Font(typeface, 20.f), std::vector<GlyphID>{glyph, glyph},
                      std::vector<float>{0.f, 0.f},
                      std::vector<float>{0.f, 0.f});
    TextBlob blob(std::move(runs));
    for (const auto& test : cases) {
      SCOPED_TRACE(::testing::Message()
                   << "emoji=" << emoji << " copy=" << test.texture_copy
                   << " native=" << test.native_blend
                   << " coherent=" << test.coherent);
      FakeGPUContext context;
      ASSERT_TRUE(context.Init());
      context.SetEnableMergingDrawCall(true);
      auto* device = context.device();
      auto caps = std::make_unique<GPUCaps>();
      caps->supports_native_advanced_blend = test.native_blend;
      caps->supports_native_advanced_blend_coherent = test.coherent;
      caps->native_blend_shader_variant = test.native_blend;
      device->InitCaps(std::move(caps));

      Paint paint;
      paint.SetBlendMode(BlendMode::kOverlay);
      paint.SetAlphaF(0.5f);
      PrecompileDraw(context, false,
                     emoji ? PrecompileDrawType::kDrawEmojiText
                           : PrecompileDrawType::kDrawText,
                     paint);
      device->set_disallow_shader_pipeline_creation(true);
      GPUSurfaceDescriptor desc{};
      desc.width = 64;
      desc.height = 64;
      desc.sample_count = 1;
      auto surface = context.CreateSurface(&desc);
      static_cast<FakeGPUSurface*>(surface.get())
          ->SetSupportsTextureCopyDstRead(test.texture_copy);
      auto* canvas = surface->LockCanvas(true);
      canvas->DrawTextBlob(&blob, 8.f, 32.f, paint);
      canvas->Flush();
      surface->Flush();

      const bool split =
          test.texture_copy && !(emoji && test.native_blend && test.coherent);
      EXPECT_EQ(device->last_instance_counts(),
                split ? (std::vector<uint32_t>{1u, 1u})
                      : (std::vector<uint32_t>{2u}));
      EXPECT_EQ(device->last_render_pass_count(),
                test.texture_copy && !(emoji && test.native_blend) ? 3u : 1u);
      EXPECT_EQ(device->disallowed_shader_function_count(), 0u);
      EXPECT_EQ(device->disallowed_render_pipeline_count(), 0u);
      EXPECT_EQ(device->disallowed_clone_pipeline_count(), 0u);
    }
  }
}

TEST(PrecompileDrawTest, TextPaintShadersHitPrecompiledPipelines) {
  auto typeface = Typeface::MakeFromFile(SKITY_FONT_DIR
                                         "fonts/resources/Roboto-Regular.ttf");
  ASSERT_NE(typeface, nullptr);
  const GlyphID glyph = typeface->UnicharToGlyph('O');
  ASSERT_NE(glyph, 0);
  std::vector<TextRun> runs;
  runs.emplace_back(Font(typeface, 16.f), std::vector<GlyphID>{glyph},
                    std::vector<float>{0.f}, std::vector<float>{0.f});
  TextBlob blob(std::move(runs));
  Paint gradient_paint;
  Point points[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  gradient_paint.SetShader(Shader::MakeLinear(points, colors, nullptr, 2));
  const Paint paints[] = {gradient_paint, MakeImagePaint(MakeTestImage())};
  for (const auto& paint : paints) {
    FakeGPUContext context;
    ASSERT_TRUE(context.Init());
    ExpectRealDrawHitsPrecompiledPipeline(
        context, PrecompileDrawType::kDrawText, paint, false,
        [&](Canvas* canvas) { canvas->DrawTextBlob(&blob, 4.f, 24.f, paint); });
    EXPECT_EQ(context.device()->last_instance_counts(),
              std::vector<uint32_t>{1u});
  }
}

TEST(PrecompileDrawTest, TextPrecompilesLegacyMaskedBlendFallback) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  auto precompile_context = MakePrecompileContext(context, false);

  Paint paint;
  paint.SetBlendMode(BlendMode::kOverlay);
  const auto pipeline_count = device->render_pipeline_count();
  precompile_context->PrecompileDraw(PrecompileDrawType::kDrawText, paint);

  // The fake device has neither framebuffer fetch nor dual-source blending:
  // precompile the normal texture-copy plan and the no-copy legacy fallback.
  EXPECT_EQ(device->render_pipeline_count() - pipeline_count, 2u);
}

TEST(PrecompileDrawTest, SupportsGradientTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, nullptr, 2));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsTextureTextDrawWithoutGPUResources) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  Paint paint = MakeImagePaint(MakeTestImage());

  PrecompileDraw(context, false, PrecompileDrawType::kDrawText, paint);

  EXPECT_TRUE(device->HasFragmentFunctionLabel("FS_Texture_TextA8"));
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsSDFTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawSDFText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
}

TEST(PrecompileDrawTest, SupportsEmojiTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
  EXPECT_TRUE(
      device->HasFragmentFunctionLabel("FS_SolidVertexColor_TextColor"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel(
      "FS_SolidVertexColor_TextColorSwizzleRB"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Emoji"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

TEST(PrecompileDrawTest, EmojiPrecompilesNoTextureCopyFallback) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();
  ASSERT_NE(device, nullptr);

  Paint paint;
  paint.SetBlendMode(BlendMode::kOverlay);
  const auto pipeline_count = device->render_pipeline_count();
  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  // RGBA and BGRA each need a texture-copy plan and its no-copy fallback.
  EXPECT_EQ(device->render_pipeline_count() - pipeline_count, 4u);
}

TEST(PrecompileDrawTest, SupportsGradientEmojiTextDraw) {
  FakeGPUContext context;
  ASSERT_TRUE(context.Init());
  auto* device = context.device();

  Paint paint;
  Point pts[] = {{0.f, 0.f, 0.f, 1.f}, {16.f, 16.f, 0.f, 1.f}};
  Vec4 colors[] = {Colors::kRed, Colors::kBlue};
  paint.SetShader(Shader::MakeLinear(pts, colors, nullptr, 2));

  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_GT(device->shader_function_count(), 0u);
  EXPECT_GT(device->render_pipeline_count(), 0u);
  EXPECT_EQ(device->texture_count(), 0u);
  EXPECT_EQ(device->sampler_count(), 0u);
  EXPECT_TRUE(
      device->HasFragmentFunctionLabel("FS_SolidVertexColor_TextColor"));
  EXPECT_TRUE(device->HasFragmentFunctionLabel(
      "FS_SolidVertexColor_TextColorSwizzleRB"));
  EXPECT_FALSE(device->HasFragmentFunctionLabelContaining("Emoji"));

  auto shader_function_count = device->shader_function_count();
  auto render_pipeline_count = device->render_pipeline_count();

  PrecompileDraw(context, false, PrecompileDrawType::kDrawEmojiText, paint);

  EXPECT_EQ(device->shader_function_count(), shader_function_count);
  EXPECT_EQ(device->render_pipeline_count(), render_pipeline_count);
}

}  // namespace skity
