// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/render/hw/draw/wgx_filter.hpp"

#include "src/effect/color_filter_base.hpp"
#include "src/gpu/gpu_render_pass.hpp"
#include "src/render/hw/draw/wgx_utils.hpp"
#include "src/render/hw/hw_draw.hpp"
#include "src/render/hw/hw_stage_buffer.hpp"

namespace skity {

std::string WGXFilterFragment::GenFunctionSignature() const {
  std::string signature = "fn filter_color";

  if (!suffix_.empty()) {
    signature += "_" + suffix_;
  }

  signature += "(input_color: vec4<f32>) -> vec4<f32>";

  return signature;
}

class WGXBlendFilter : public WGXFilterFragment {
 public:
  WGXBlendFilter(Color color, BlendMode mode)
      : WGXFilterFragment(""), color_(color), mode_(mode) {}

  WGXBlendFilter(std::string suffix, Color color, BlendMode mode)
      : WGXFilterFragment(std::move(suffix)), color_(color), mode_(mode) {}

  ~WGXBlendFilter() override = default;

  uint32_t InitBinding(uint32_t binding) override {
    binding_ = binding;
    return binding + 1;
  }

  std::string GetShaderName() const override {
    switch (mode_) {
      case BlendMode::kClear:
        return "BlendClearFilter";
      case BlendMode::kSrc:
        return "BlendSrcFilter";
      case BlendMode::kDst:
        return "BlendDstFilter";
      case BlendMode::kSrcOver:
        return "BlendSrcOverFilter";
      case BlendMode::kDstOver:
        return "BlendDstOverFilter";
      case BlendMode::kSrcIn:
        return "BlendSrcInFilter";
      case BlendMode::kDstIn:
        return "BlendDstInFilter";
      case BlendMode::kSrcOut:
        return "BlendSrcOutFilter";
      case BlendMode::kDstOut:
        return "BlendDstOutFilter";
      case BlendMode::kSrcATop:
        return "BlendSrcATopFilter";
      case BlendMode::kDstATop:
        return "BlendDstATopFilter";
      case BlendMode::kXor:
        return "BlendXorFilter";
      case BlendMode::kPlus:
        return "BlendPlusFilter";
      case BlendMode::kModulate:
        return "BlendModulateFilter";
      case BlendMode::kScreen:
        return "BlendScreenFilter";
      default:
        return "UnsupportedBlendFilter";
    }
  }

  std::string GenSourceWGSL() const override {
    std::string wgsl_source = "";

    if (mode_ != BlendMode::kClear && mode_ != BlendMode::kDst) {
      wgsl_source += "@group(1) @binding(" + std::to_string(binding_) +
                     ") var<uniform> uBlendSrcColor";

      if (!suffix_.empty()) {
        wgsl_source += "_" + suffix_;
      }

      wgsl_source += " : vec4<f32>;\n";
    }

    wgsl_source += GenFunctionSignature();
    wgsl_source += "{\n";

    if (!suffix_.empty()) {
      wgsl_source +=
          "var uBlendSrcColor : vec4<f32> = uBlendSrcColor_" + suffix_ + ";\n";
    }

    switch (mode_) {
      case BlendMode::kClear:
        wgsl_source += R"(
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        )";
        break;
      case BlendMode::kSrc:
        wgsl_source += R"(
            return uBlendSrcColor;
        )";
        break;
      case BlendMode::kDst:
        wgsl_source += R"(
            return input_color;
        )";
        break;
      case BlendMode::kSrcOver:
        wgsl_source += R"(
            return uBlendSrcColor + input_color * (1.0 - uBlendSrcColor.a);
        )";
        break;
      case BlendMode::kDstOver:
        wgsl_source += R"(
            return input_color + uBlendSrcColor * (1.0 - input_color.a);
        )";
        break;
      case BlendMode::kSrcIn:
        wgsl_source += R"(
            return uBlendSrcColor * input_color.a;
        )";
        break;
      case BlendMode::kDstIn:
        wgsl_source += R"(
            return input_color * uBlendSrcColor.a;
        )";
        break;
      case BlendMode::kSrcOut:
        wgsl_source += R"(
            return uBlendSrcColor * (1.0 - input_color.a);
        )";
        break;
      case BlendMode::kDstOut:
        wgsl_source += R"(
            return input_color * (1.0 - uBlendSrcColor.a);
        )";
        break;
      case BlendMode::kSrcATop:
        wgsl_source += R"(
            return uBlendSrcColor * input_color.a + input_color * (1.0 - uBlendSrcColor.a);
        )";
        break;
      case BlendMode::kDstATop:
        wgsl_source += R"(
            return uBlendSrcColor.a * input_color + uBlendSrcColor * (1.0 - input_color.a);
        )";
        break;
      case BlendMode::kXor:
        wgsl_source += R"(
            return uBlendSrcColor * (1.0 - input_color.a) + input_color * (1.0 - uBlendSrcColor.a);
        )";
        break;
      case BlendMode::kPlus:
        wgsl_source += R"(
            return min(uBlendSrcColor + input_color, vec4<f32>(1.0));
        )";
        break;
      case BlendMode::kModulate:
        wgsl_source += R"(
            return uBlendSrcColor * input_color;
        )";
        break;
      case BlendMode::kScreen:
        wgsl_source += R"(
            return uBlendSrcColor + input_color - uBlendSrcColor * input_color;
        )";
        break;
      default:
        wgsl_source += R"(
            return vec4<f32>(0.0, 0.0, 0.0, 0.0);
        )";
        break;
    }
    wgsl_source += "}\n";

    return wgsl_source;
  }

  void SetupBindGroup(Command* cmd, HWDrawContext* context) override {
    if (mode_ == BlendMode::kClear || mode_ == BlendMode::kDst) {
      return;
    }

    if (cmd->pipeline == nullptr) {
      return;
    }

    auto group = cmd->pipeline->GetBindingGroup(1);
    if (group == nullptr) {
      return;
    }

    auto entry = group->GetEntry(binding_);

    if (entry == nullptr || entry->type != wgx::BindingType::kUniformBuffer ||
        entry->type_definition->name != "vec4<f32>") {
      return;
    }

    auto color4f = Color4fFromColor(color_);
    color4f[0] *= color4f[3];
    color4f[1] *= color4f[3];
    color4f[2] *= color4f[3];
    entry->type_definition->SetData(&color4f, sizeof(float) * 4);

    UploadBindGroup(group->group, entry, cmd, context);
  }

 private:
  Color color_;
  BlendMode mode_;
  mutable uint32_t binding_ = 0;
};

class WGXMatrixFilter : public WGXFilterFragment {
 public:
  WGXMatrixFilter(Vec4 matrix_add, Matrix matrix_mul)
      : WGXFilterFragment(""),
        matrix_add_(matrix_add),
        matrix_mul_(matrix_mul) {}

  WGXMatrixFilter(std::string suffix, Vec4 matrix_add, Matrix matrix_mul)
      : WGXFilterFragment(std::move(suffix)),
        matrix_add_(matrix_add),
        matrix_mul_(matrix_mul) {}

  ~WGXMatrixFilter() override = default;

  std::string GetShaderName() const override { return "MatrixFilter"; }

  uint32_t InitBinding(uint32_t binding) override {
    binding_ = binding;
    return binding + 1;
  }

  std::string GenSourceWGSL() const override {
    std::string wgsl_source = "struct MatrixFilterInfo";

    if (!suffix_.empty()) {
      wgsl_source += "_" + suffix_;
    }

    wgsl_source += R"(
        {
            matrix_add : vec4<f32>,
            matrix_mul : mat4x4<f32>,
        };
    )";

    wgsl_source += "@group(1) @binding(" + std::to_string(binding_) +
                   ") var<uniform> uMatrixFilterInfo";

    if (!suffix_.empty()) {
      wgsl_source += "_" + suffix_ + " : MatrixFilterInfo";
    } else {
      wgsl_source += " : MatrixFilterInfo";
    }

    if (!suffix_.empty()) {
      wgsl_source += "_" + suffix_ + ";\n";
    } else {
      wgsl_source += ";\n";
    }

    wgsl_source += GenFunctionSignature();
    wgsl_source += "{\n";
    if (!suffix_.empty()) {
      wgsl_source += "var uMatrixFilterInfo : MatrixFilterInfo_" + suffix_ +
                     " = uMatrixFilterInfo_" + suffix_ + ";\n";
    }
    wgsl_source += R"(
            if input_color.a > 0.0 {
               input_color.rgb /= input_color.a;
            }

            var color: vec4<f32> = uMatrixFilterInfo.matrix_mul * input_color + uMatrixFilterInfo.matrix_add;

            return vec4<f32>(color.rgb * color.a, color.a);
        }
    )";

    return wgsl_source;
  }

  void SetupBindGroup(Command* cmd, HWDrawContext* context) override {
    if (cmd->pipeline == nullptr) {
      return;
    }
    auto group = cmd->pipeline->GetBindingGroup(1);
    if (group == nullptr) {
      return;
    }
    auto entry = group->GetEntry(binding_);

    std::string name = "MatrixFilterInfo";

    if (!suffix_.empty()) {
      name += "_" + suffix_;
    }

    if (entry == nullptr || entry->type != wgx::BindingType::kUniformBuffer ||
        entry->type_definition->name != name) {
      return;
    }

    auto info_struct =
        static_cast<wgx::StructDefinition*>(entry->type_definition.get());
    info_struct->GetMember("matrix_add")
        ->type->SetData(&matrix_add_, sizeof(float) * 4);
    info_struct->GetMember("matrix_mul")
        ->type->SetData(&matrix_mul_, sizeof(float) * 16);

    UploadBindGroup(group->group, entry, cmd, context);
  }

 private:
  Vec4 matrix_add_ = {};
  Matrix matrix_mul_ = {};
  mutable uint32_t binding_ = 0;
};

class WGXGammaFilter : public WGXFilterFragment {
 public:
  explicit WGXGammaFilter(ColorFilterType type)
      : WGXFilterFragment(""), type_(type) {}

  WGXGammaFilter(std::string suffix, ColorFilterType type)
      : WGXFilterFragment(std::move(suffix)), type_(type) {}

  ~WGXGammaFilter() override = default;

  uint32_t InitBinding(uint32_t binding) override { return binding; }

  std::string GetShaderName() const override {
    if (type_ == ColorFilterType::kLinearToSRGBGamma) {
      return "LinearToSRGBGammaFilter";
    } else if (type_ == ColorFilterType::kSRGBToLinearGamma) {
      return "SRGBToLinearGammaFilter";
    } else {
      return "Unknown";
    }
  }

  std::string GenSourceWGSL() const override {
    std::string wgsl_source = GenFunctionSignature();

    if (type_ == ColorFilterType::kLinearToSRGBGamma) {
      wgsl_source += R"(
        {
            for (var i: int = 0; i < 3; i++) {
                if input_color[i] <= 0.0031308 {
                    input_color[i] *= 12.92;
                } else {
                    input_color[i] = 1.055 * pow(input_color[i], 1.0 / 2.4) - 0.055;
                }
            }

            return input_color;
        }
      )";
    } else if (type_ == ColorFilterType::kSRGBToLinearGamma) {
      wgsl_source += R"(
        {
            for (var i: int = 0; i < 3; i++) {
                if input_color[i] <= 0.04045 {
                    input_color[i] /= 12.92;
                } else {
                    input_color[i] = pow((input_color[i] + 0.055) / 1.055, 2.4);
                }
            }
            return input_color;
        }
      )";
    }

    return wgsl_source;
  }

  void SetupBindGroup(Command* cmd, HWDrawContext* context) override {}

 private:
  ColorFilterType type_ = ColorFilterType::kLinearToSRGBGamma;
};

class WGXComposeFilter : public WGXFilterFragment {
 public:
  explicit WGXComposeFilter(const std::vector<ColorFilter*>& filters)
      : WGXFilterFragment("") {
    size_t index = 0;
    for (auto& filter : filters) {
      if (filter == nullptr) {
        continue;
      }

      filters_.emplace_back(
          WGXFilterFragment::Make(filter, std::to_string(index)));

      index += 1;
    }
  }

  ~WGXComposeFilter() override = default;

  uint32_t InitBinding(uint32_t binding) override {
    for (auto& filter : filters_) {
      binding = filter->InitBinding(binding);
    }
    return binding;
  }

  std::string GetShaderName() const override {
    std::string name = "ComposeFilter";

    for (auto& filter : filters_) {
      name += "_" + filter->GetShaderName();
    }

    return name;
  }

  std::string GenSourceWGSL() const override {
    std::string wgsl_source = "";
    for (auto& filter : filters_) {
      wgsl_source += filter->GenSourceWGSL();
    }

    wgsl_source += GenFunctionSignature();

    wgsl_source += "{\n";

    for (size_t i = 0; i < filters_.size(); i++) {
      wgsl_source += "input_color = filter_color_" + std::to_string(i) +
                     "(input_color);\n";
    }

    wgsl_source += "return input_color;\n";
    wgsl_source += "}\n";

    return wgsl_source;
  }

  void SetupBindGroup(Command* cmd, HWDrawContext* context) override {
    for (auto& filter : filters_) {
      filter->SetupBindGroup(cmd, context);
    }
  }

 private:
  std::vector<std::unique_ptr<WGXFilterFragment>> filters_;
};

std::unique_ptr<WGXFilterFragment> WGXFilterFragment::Make(ColorFilter* filter,
                                                           std::string suffix) {
  auto filter_base = As_CFB(filter);

  if (filter_base->GetType() == ColorFilterType::kBlend) {
    auto blend_filter = static_cast<BlendColorFilter*>(filter_base);
    return std::make_unique<WGXBlendFilter>(suffix, blend_filter->GetColor(),
                                            blend_filter->GetBlendMode());
  } else if (filter_base->GetType() == ColorFilterType::kLinearToSRGBGamma ||
             filter_base->GetType() == ColorFilterType::kSRGBToLinearGamma) {
    return std::make_unique<WGXGammaFilter>(suffix, filter_base->GetType());
  } else if (filter_base->GetType() == ColorFilterType::kMatrix) {
    auto matrix_filter = static_cast<MatrixColorFilter*>(filter_base);

    Matrix matrix_mul;
    Vec4 matrix_add;

    std::tie(matrix_mul, matrix_add) = matrix_filter->GetMatrix();

    return std::make_unique<WGXMatrixFilter>(suffix, matrix_add, matrix_mul);
  } else if (filter_base->GetType() == ColorFilterType::kCompose) {
    auto compose_filter = static_cast<ComposeColorFilter*>(filter_base);

    return std::make_unique<WGXComposeFilter>(compose_filter->GetFilters());
  }

  return {};
}

}  // namespace skity
