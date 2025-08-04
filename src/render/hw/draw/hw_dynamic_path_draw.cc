// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/hw_dynamic_path_draw.hpp"

#include "src/effect/pixmap_shader.hpp"
#include "src/gpu/gpu_context_impl.hpp"
#include "src/render/hw/draw/fragment/wgsl_gradient_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_solid_color.hpp"
#include "src/render/hw/draw/fragment/wgsl_stencil_fragment.hpp"
#include "src/render/hw/draw/fragment/wgsl_texture_fragment.hpp"
#include "src/render/hw/draw/geometry/wgsl_gradient_path.hpp"
#include "src/render/hw/draw/geometry/wgsl_path_geometry.hpp"
#include "src/render/hw/draw/geometry/wgsl_texture_path.hpp"
#include "src/render/hw/draw/step/color_step.hpp"
#include "src/render/hw/draw/step/stencil_step.hpp"
#include "src/render/hw/draw/wgx_filter.hpp"

namespace skity {

HWDynamicPathDraw::HWDynamicPathDraw(Matrix transform, Path path, Paint paint,
                                     bool is_stroke)
    : HWDynamicDraw(transform, paint.GetBlendMode()),
      path_(std::move(path)),
      paint_(std::move(paint)),
      is_stroke_(is_stroke) {}

void HWDynamicPathDraw::OnGenerateDrawStep(ArrayList<HWDrawStep *, 2> &steps,
                                           HWDrawContext *context) {
  bool single_pass = !is_stroke_ && path_.IsConvex() && !paint_.IsAntiAlias();

  auto geom = GenGeometry(context, false);

  auto frag = GenFragment(context);

  if (paint_.GetColorFilter()) {
    frag->SetFilter(WGXFilterFragment::Make(paint_.GetColorFilter().get()));
  }

  CoverageType coverage = CoverageType::kNone;

  if (single_pass) {
    coverage = CoverageType::kNone;
  } else if (is_stroke_) {
    coverage = CoverageType::kNoZero;
  } else if (path_.GetFillType() == Path::PathFillType::kEvenOdd) {
    coverage = CoverageType::kEvenOdd;
  } else {
    coverage = CoverageType::kWinding;
  }

  if (!single_pass) {
    // need stencil step first
    steps.emplace_back(context->arena_allocator->Make<StencilStep>(
        context->arena_allocator->Make<WGSLPathGeometry>(path_, paint_,
                                                         is_stroke_, false),
        context->arena_allocator->Make<WGSLStencilFragment>(),
        coverage == CoverageType::kNoZero));
  }

  if (paint_.IsAntiAlias()) {
    auto geometry = GenGeometry(context, true);

    auto fragment = GenFragment(context);

    if (paint_.GetColorFilter()) {
      fragment->SetFilter(
          WGXFilterFragment::Make(paint_.GetColorFilter().get()));
    }

    fragment->SetAntiAlias(true);

    steps.emplace_back(context->arena_allocator->Make<ColorAAStep>(
        std::move(geometry), std::move(fragment), coverage));
  }

  steps.emplace_back(context->arena_allocator->Make<ColorStep>(
      std::move(geom), std::move(frag), coverage));
}

HWWGSLGeometry *HWDynamicPathDraw::GenGeometry(HWDrawContext *context,
                                               bool aa) const {
  auto arena_allocator = context->arena_allocator;
  if (paint_.GetShader()) {
    // gradient or image
    auto type = paint_.GetShader()->AsGradient(nullptr);

    if (type == Shader::GradientType::kNone) {
      auto pixmap_shader =
          std::static_pointer_cast<PixmapShader>(paint_.GetShader());

      const std::shared_ptr<Image> &image = *(pixmap_shader->AsImage());

      Matrix inv_local_matrix{};

      pixmap_shader->GetLocalMatrix().Invert(&inv_local_matrix);

      return arena_allocator->Make<WGSLTexturePath>(
          path_, paint_, is_stroke_, aa, inv_local_matrix,
          static_cast<float>(image->Width()),
          static_cast<float>(image->Height()));
    } else {
      return arena_allocator->Make<WGSLGradientPath>(
          path_, paint_, is_stroke_, aa, paint_.GetShader()->GetLocalMatrix());
    }

  } else {
    return arena_allocator->Make<WGSLPathGeometry>(path_, paint_, is_stroke_,
                                                   aa);
  }
}

HWWGSLFragment *HWDynamicPathDraw::GenFragment(HWDrawContext *context) const {
  auto arena_allocator = context->arena_allocator;
  if (paint_.GetShader()) {
    auto type = paint_.GetShader()->AsGradient(nullptr);

    if (type == Shader::GradientType::kNone) {
      // handle image rendering in the future
      auto pixmap_shader =
          std::static_pointer_cast<PixmapShader>(paint_.GetShader());

      const std::shared_ptr<Image> &image = *(pixmap_shader->AsImage());

      std::shared_ptr<GPUTexture> texture;
      if (image->GetTexture()) {
        const auto &texture_image = *(image->GetTexture());
        texture = texture_image->GetGPUTexture();
      } else if (image->GetPixmap()) {
        const auto &pixmap_image = *(image->GetPixmap());
        auto texture_handler =
            context->gpuContext->GetTextureManager()->FindOrCreateTexture(
                Texture::FormatFromColorType(pixmap_image->GetColorType()),
                pixmap_image->Width(), pixmap_image->Height(),
                pixmap_image->GetAlphaType(), pixmap_image);
        texture_handler->UploadImage(pixmap_image);
        texture = texture_handler->GetGPUTexture();
      } else {
        auto texture_handler = image->GetTextureByContext(context->gpuContext);

        if (texture_handler) {
          texture = texture_handler->GetGPUTexture();
        }
      }

      if (texture != nullptr) {
        GPUSamplerDescriptor descriptor;
        descriptor.mag_filter =
            ToGPUFilterMode(pixmap_shader->GetSamplingOptions()->filter);
        descriptor.min_filter =
            ToGPUFilterMode(pixmap_shader->GetSamplingOptions()->filter);
        descriptor.mipmap_filter =
            ToGPUMipmapMode(pixmap_shader->GetSamplingOptions()->mipmap);
        auto sampler =
            context->gpuContext->GetGPUDevice()->CreateSampler(descriptor);

        return arena_allocator->Make<WGSLTextureFragment>(
            pixmap_shader, texture, sampler, paint_.GetAlphaF());
      } else {
        return arena_allocator->Make<WGSLSolidColor>(Colors::kRed);
      }

    } else {
      Shader::GradientInfo info{};

      paint_.GetShader()->AsGradient(&info);

      return arena_allocator->Make<WGSLGradientFragment>(info, type,
                                                         paint_.GetAlphaF());
    }

  } else {
    if (is_stroke_) {
      return arena_allocator->Make<WGSLSolidColor>(paint_.GetStrokeColor());
    } else {
      return arena_allocator->Make<WGSLSolidColor>(paint_.GetFillColor());
    }
  }
}

}  // namespace skity
