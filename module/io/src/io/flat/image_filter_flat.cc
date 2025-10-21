// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <optional>
#include <vector>

#include "src/io/flat/blender_flat.hpp"
#include "src/io/memory_read.hpp"
#include "src/picture_priv.hpp"

namespace skity {

namespace {

struct Common {
  bool ReadCommon(ReadBuffer& buffer, int32_t expect_inputs);

  std::optional<Rect> crop_rect = {};
  std::vector<std::shared_ptr<ImageFilter>> inputs = {};
};

bool Common::ReadCommon(ReadBuffer& buffer, int32_t expect_inputs) {
  auto count = buffer.ReadInt();

  if (!buffer.Validate(count >= 0)) {
    return false;
  }

  if (!buffer.Validate(expect_inputs < 0 || count == expect_inputs)) {
    return false;
  }

  for (int32_t i = 0; i < count; i++) {
    if (buffer.ReadBool()) {
      inputs.emplace_back(buffer.ReadImageFilter());
    } else {
      inputs.emplace_back(nullptr);
    }

    if (!buffer.IsValid()) {
      return false;
    }
  }

  if (buffer.IsVersionLT(Version::kRemoveDeprecatedCropRect)) {
    static constexpr uint32_t kHasAll_CropEdge = 0xF;

    crop_rect = buffer.ReadRect();

    if (!buffer.IsValid()) {
      return false;
    }

    auto flags = buffer.ReadU32();

    if (!buffer.IsValid() ||
        !buffer.Validate(flags == 0x0 || flags == kHasAll_CropEdge)) {
      return false;
    }

    if (flags != kHasAll_CropEdge) {
      crop_rect.reset();
    }
  }

  return buffer.IsValid();
}

void SkipBlendImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 2)) {
    return;
  }

  auto mode = buffer.ReadU32();

  static constexpr uint32_t kCustom_SkBlendMode = 0xFF;
  static constexpr uint32_t kArithmetic_SkBlendMode = kCustom_SkBlendMode + 1;

  if (mode == kArithmetic_SkBlendMode) {
    if (buffer.Validate(
            !buffer.IsVersionLT(Version::kCombineBlendArithmeticFilters))) {
      for (int32_t i = 0; i < 4; i++) {
        (void)buffer.ReadFloat();
      }

      (void)buffer.ReadBool();
    }
  } else if (mode == kCustom_SkBlendMode) {
    BlenderModeFlattenable::SkipReadBlender(buffer);
  }

  // no validate
}

void SkipLegacyArithmeticImageFilter(ReadBuffer& buffer) {
  if (!buffer.Validate(
          buffer.IsVersionLT(Version::kCombineBlendArithmeticFilters))) {
    return;
  }

  Common common;
  if (!common.ReadCommon(buffer, 2)) {
    return;
  }

  for (int32_t i = 0; i < 4; i++) {
    (void)buffer.ReadFloat();
  }

  (void)buffer.ReadBool();
}

std::shared_ptr<ImageFilter> ReadBlurImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return {};
  }

  auto sigma_x = buffer.ReadFloat();
  auto sigma_y = buffer.ReadFloat();

  auto tile_mode = static_cast<TileMode>(buffer.ReadU32());
  (void)tile_mode;  // skip tile_mode

  return ImageFilters::Blur(sigma_x, sigma_y);
}

std::shared_ptr<ImageFilter> ReadColorFilterImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return {};
  }

  auto color_filter = buffer.ReadColorFilter();

  auto outer = ImageFilters::ColorFilter(color_filter);

  if (common.inputs[0] != nullptr) {
    // compose color filter with input

    return ImageFilters::Compose(outer, common.inputs[0]);
  }

  return outer;
}

std::shared_ptr<ImageFilter> ReadComposeImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 2)) {
    return {};
  }

  if (common.inputs[0] == nullptr && common.inputs[1] == nullptr) {
    return {};
  }

  auto outer = common.inputs[0];
  auto inner = common.inputs[1];

  return ImageFilters::Compose(outer, inner);
}

void SkipCropImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return;
  }

  (void)buffer.ReadRect();

  if (!buffer.IsVersionLT(Version::kCropImageFilterSupportsTiling)) {
    (void)buffer.ReadU32();  // tile_mode
  }
}

void SkipLegacyTileImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return;
  }

  (void)buffer.ReadRect();  // src
  (void)buffer.ReadRect();  // dst
}

void SkipDisplacementMapImageFilter(ReadBuffer& buffer) {
  Common common;

  if (!common.ReadCommon(buffer, 2)) {
    return;
  }

  (void)buffer.ReadU32();    // xsel
  (void)buffer.ReadU32();    // ysel
  (void)buffer.ReadFloat();  // scale
}

void SkipImageImageFilter(ReadBuffer& buffer) {
  if (buffer.IsVersionLT(Version::kImageFilterImageSampling_Version)) {
    (void)buffer.ReadU32();  // quality value
  } else {
    (void)buffer.ReadSamplingOptions();
  }

  (void)buffer.ReadRect();  // src
  (void)buffer.ReadRect();  // dst

  (void)buffer.ReadImage();
}

void SkipLightingImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return;
  }

  (void)buffer.ReadU32();    // light type;
  (void)buffer.ReadColor();  // light color

  Vec3 pt;

  buffer.ReadPad32(&pt, sizeof(pt));  // light position
  buffer.ReadPad32(&pt, sizeof(pt));  // light direction

  (void)buffer.ReadFloat();  // fall off exponent
  (void)buffer.ReadFloat();  // cos cut off

  (void)buffer.ReadU32();    // material type
  (void)buffer.ReadFloat();  // surface depth
  (void)buffer.ReadFloat();  // k
  (void)buffer.ReadFloat();  // shininess
}

void SkipLegacyLightingImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return;
  }

  // skip legacy light data
  {
    auto type = buffer.ReadU32();  // light type

    // skip light color with RGB float value
    (void)buffer.ReadFloat();
    (void)buffer.ReadFloat();
    (void)buffer.ReadFloat();

    if (type == 0) {  // Distant Light
      // skip light direction with Vec3 float value
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
    } else if (type == 1) {  // Point Light
      // skip light position with Vec3 float value
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
    } else if (type == 2) {  // Spot Light
      // skip light position with Vec3 float value
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
      // skip light direction with Vec3 float value
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();
      (void)buffer.ReadFloat();

      (void)buffer.ReadFloat();  // fall off exponent
      (void)buffer.ReadFloat();  // cos cut off

      (void)buffer.ReadFloat();  // skip cos inner cone angle
      (void)buffer.ReadFloat();  // skip cone scale
      (void)buffer.ReadFloat();  // skip s
      (void)buffer.ReadFloat();  // ""
      (void)buffer.ReadFloat();  // ""
    }
  }

  (void)buffer.ReadFloat();  // skip surface scale;
  (void)buffer.ReadFloat();  // skip ks
  (void)buffer.ReadFloat();  // skip shininess
}

void SkipMagnifierImageFilter(ReadBuffer& buffer) {
  if (buffer.IsVersionLT(Version::kRevampMagnifierFilter)) {
    return;
  }

  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return;
  }

  (void)buffer.ReadRect();   // lens rect
  (void)buffer.ReadFloat();  // zoom amount
  (void)buffer.ReadFloat();  // inset
  (void)buffer.ReadSamplingOptions();
}

void SkipMatrixConvolutionImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return;
  }

  auto width = buffer.ReadInt();   // kernel width
  auto height = buffer.ReadInt();  // kernel height

  auto count = buffer.ReadInt();

  auto kernel_area = static_cast<int64_t>(width) * static_cast<int64_t>(height);

  if (!buffer.Validate(kernel_area == count)) {
    return;
  }

  if (!buffer.ValidateCanReadN<float>(count)) {
    return;
  }

  std::vector<float> kernel(count);

  if (!buffer.ReadArrayN<float>(kernel.data(), count)) {
    return;
  }

  (void)buffer.ReadFloat();  // gain
  (void)buffer.ReadFloat();  // bias

  (void)buffer.ReadInt();  // kernel offset x
  (void)buffer.ReadInt();  // kernel offset y

  if (buffer.IsVersionLT(Version::kConvolutionImageFilterTilingUpdate)) {
    (void)buffer.ReadInt();  // tile mode
  }

  (void)buffer.ReadBool();  // convolve alpha
}

std::shared_ptr<ImageFilter> ReadMatrixTransformImageFilter(
    ReadBuffer& buffer) {
  Common common;

  if (!common.ReadCommon(buffer, 1)) {
    return {};
  }

  auto matrix = buffer.ReadMatrix();

  if (!matrix.has_value()) {
    return {};
  }

  SamplingOptions sampling{};

  if (buffer.IsVersionLT(Version::kMatrixImageFilterSampling_Version)) {
    auto fq = buffer.ReadInt();
    if (fq == 1) {
      sampling.filter = FilterMode::kLinear;
    } else if (fq == 2) {
      sampling.filter = FilterMode::kNearest;
    }
  } else {
    sampling = buffer.ReadSamplingOptions();
  }

  auto filter = ImageFilters::MatrixTransform(matrix.value());

  if (common.inputs[0] != nullptr) {
    return ImageFilters::Compose(filter, common.inputs[0]);
  }

  return filter;
}

std::shared_ptr<ImageFilter> ReadOffsetImageFilter(ReadBuffer& buffer) {
  Common common;

  if (!common.ReadCommon(buffer, 1)) {
    return {};
  }

  auto offset = buffer.ReadPoint();

  Matrix matrix = Matrix::Translate(offset.x, offset.y);

  auto filter = ImageFilters::MatrixTransform(matrix);

  if (common.inputs[0] != nullptr) {
    return ImageFilters::Compose(filter, common.inputs[0]);
  }

  return filter;
}

void SkipMergeImageFilter(ReadBuffer& buffer) {
  Common common;

  common.ReadCommon(buffer, -1);
}

std::shared_ptr<ImageFilter> ReadMorphologyImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 1)) {
    return {};
  }

  auto width = buffer.ReadFloat();   // width
  auto height = buffer.ReadFloat();  // height
  auto type = buffer.ReadInt();      // type

  std::shared_ptr<ImageFilter> filter;

  if (type == 1) {  // Dilate
    filter = ImageFilters::Dilate(width, height);
  } else if (type == 0) {  // Erode
    filter = ImageFilters::Erode(width, height);
  }

  if (common.inputs[0] != nullptr) {
    return ImageFilters::Compose(filter, common.inputs[0]);
  }

  return filter;
}

void SkipPictureImageFilter(ReadBuffer& buffer) {
  if (buffer.ReadBool()) {
    SkipPictureInBuffer(buffer);
  }

  (void)buffer.ReadRect();  // cull rect
}

void SkipRuntimeImageFilter(ReadBuffer& buffer) {
  // this Filter nees to compile SKSL shader, we can not just skip it
  // mark the buffer as invalid and the parser will stop reading
  buffer.Validate(false);
}

void SkipShaderImageFilter(ReadBuffer& buffer) {
  Common common;
  if (!common.ReadCommon(buffer, 0)) {
    return;
  }

  if (buffer.IsVersionLT(Version::kShaderImageFilterSerializeShader)) {
    (void)buffer.ReadPaint();
  } else {
    (void)buffer.ReadShader();  // shader
    (void)buffer.ReadBool();    // dither
  }
}

std::shared_ptr<ImageFilter> ReadLocalMatrixImageFilter(ReadBuffer& buffer) {
  Common common;

  if (!common.ReadCommon(buffer, 1)) {
    return {};
  }

  auto matrix = buffer.ReadMatrix();

  if (!matrix.has_value() || common.inputs[0] == nullptr) {
    return {};
  }

  return ImageFilters::LocalMatrix(common.inputs[0], matrix.value());
}

std::shared_ptr<ImageFilter> ReadDropShadowImageFilter(ReadBuffer& buffer) {
  if (!buffer.IsVersionLT(Version::kDropShadowImageFilterComposition)) {
    return {};
  }

  Common common;
  common.ReadCommon(buffer, 1);

  auto dx = buffer.ReadFloat();
  auto dy = buffer.ReadFloat();
  auto sigma_x = buffer.ReadFloat();
  auto sigma_y = buffer.ReadFloat();

  auto color = buffer.ReadColor();

  auto shadow_only = buffer.ReadU32();

  return ImageFilters::DropShadow(dx, dy, sigma_x, sigma_y, color,
                                  common.inputs[0]);
}

}  // namespace

std::shared_ptr<Flattenable> ReadImageFilterFromMemory(
    const std::string& factory, ReadBuffer& buffer) {
  if (factory == "SkBlendImageFilter" ||
      factory == "SkXfermodeImageFilter_Base" ||
      factory == "SkXfermodeImageFilterImpl") {
    SkipBlendImageFilter(buffer);
  } else if (factory == "ArithmeticImageFilterImpl" ||
             factory == "SkArithmeticImageFilter") {
    SkipLegacyArithmeticImageFilter(buffer);
  } else if (factory == "SkBlurImageFilter" ||
             factory == "SkBlurImageFilterImpl") {
    return ReadBlurImageFilter(buffer);
  } else if (factory == "SkColorFilterImageFilter" ||
             factory == "SkColorFilterImageFilterImpl") {
    return ReadColorFilterImageFilter(buffer);
  } else if (factory == "SkComposeImageFilter" ||
             factory == "SkComposeImageFilterImpl") {
    return ReadComposeImageFilter(buffer);
  } else if (factory == "SkCropImageFilter") {
    SkipCropImageFilter(buffer);
  } else if (factory == "SkTileImageFilter" ||
             factory == "SkTileImageFilterImpl") {
    SkipLegacyTileImageFilter(buffer);
  } else if (factory == "SkDisplacementMapImageFilter" ||
             factory == "SkDisplacementMapEffect" ||
             factory == "SkDisplacementMapEffectImpl") {
    SkipDisplacementMapImageFilter(buffer);
  } else if (factory == "SkImageImageFilter" ||
             factory == "SkImageSourceImpl") {
    SkipImageImageFilter(buffer);
  } else if (factory == "SkLightingImageFilter") {
    SkipLightingImageFilter(buffer);
  } else if (factory == "SkDiffuseLightingImageFilter" ||
             factory == "SkSpecularLightingImageFilter") {
    SkipLegacyLightingImageFilter(buffer);
  } else if (factory == "SkMagnifierImageFilter") {
    SkipMagnifierImageFilter(buffer);
  } else if (factory == "SkMatrixConvolutionImageFilter" ||
             factory == "SkMatrixConvolutionImageFilterImpl") {
    SkipMatrixConvolutionImageFilter(buffer);
  } else if (factory == "SkMatrixTransformImageFilter" ||
             factory == "SkMatrixImageFilter") {
    return ReadMatrixTransformImageFilter(buffer);
  } else if (factory == "SkOffsetImageFilter" ||
             factory == "SkOffsetImageFilterImpl") {
    return ReadOffsetImageFilter(buffer);
  } else if (factory == "SkMergeImageFilter" ||
             factory == "SkMergeImageFilterImpl") {
    SkipMergeImageFilter(buffer);
  } else if (factory == "SkMorphologyImageFilter" ||
             factory == "SkMorphologyImageFilterImpl") {
    return ReadMorphologyImageFilter(buffer);
  } else if (factory == "SkPictureImageFilter" ||
             factory == "SkPictureImageFilterImpl") {
    SkipPictureImageFilter(buffer);
  } else if (factory == "SkRuntimeImageFilter") {
    SkipRuntimeImageFilter(buffer);
  } else if (factory == "SkShaderImageFilter" ||
             factory == "SkPaintImageFilter" ||
             factory == "SkPaintImageFilterImpl") {
    SkipShaderImageFilter(buffer);
  } else if (factory == "SkLocalMatrixImageFilter") {
    return ReadLocalMatrixImageFilter(buffer);
  } else if (factory == "SkDropShadowImageFilter" ||
             factory == "SkDropShadowImageFilterImpl") {
    return ReadDropShadowImageFilter(buffer);
  }

  return {};
}

FactoryProc GetImageFilterFactoryProc(const std::string& factory_name) {
  static std::vector<std::string_view> kImageFilterFactories = {
      "SkBlendImageFilter",
      "SkXfermodeImageFilter_Base",
      "SkXfermodeImageFilterImpl",
      "ArithmeticImageFilterImpl",
      "SkArithmeticImageFilter",
      "SkBlurImageFilter",
      "SkBlurImageFilterImpl",
      "SkColorFilterImageFilter",
      "SkColorFilterImageFilterImpl",
      "SkComposeImageFilter",
      "SkComposeImageFilterImpl",
      "SkCropImageFilter",
      "SkTileImageFilter",
      "SkTileImageFilterImpl",
      "SkDisplacementMapImageFilter",
      "SkDisplacementMapEffect",
      "SkDisplacementMapEffectImpl",
      "SkImageImageFilter",
      "SkImageSourceImpl",
      "SkLightingImageFilter",
      "SkDiffuseLightingImageFilter",
      "SkSpecularLightingImageFilter",
      "SkMagnifierImageFilter",
      "SkMatrixConvolutionImageFilter",
      "SkMatrixConvolutionImageFilterImpl",
      "SkMatrixTransformImageFilter",
      "SkMatrixImageFilter",
      "SkOffsetImageFilter",
      "SkOffsetImageFilterImpl",
      "SkMergeImageFilter",
      "SkMergeImageFilterImpl",
      "SkMorphologyImageFilter",
      "SkMorphologyImageFilterImpl",
      "SkPictureImageFilter",
      "SkPictureImageFilterImpl",
      "SkRuntimeImageFilter",
      "SkShaderImageFilter",
      "SkPaintImageFilter",
      "SkPaintImageFilterImpl",
      "SkLocalMatrixImageFilter",
      "SkDropShadowImageFilter",
      "SkDropShadowImageFilterImpl",
  };

  auto it = std::find(kImageFilterFactories.begin(),
                      kImageFilterFactories.end(), factory_name);

  if (it != kImageFilterFactories.end()) {
    return ReadImageFilterFromMemory;
  }

  return nullptr;
}

}  // namespace skity
