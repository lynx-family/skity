// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <gtest/gtest.h>

#include <skity/text/font_style.hpp>
#include <skity/text/typeface.hpp>

#include "src/render/text/sdf_gen.hpp"
#include "src/render/text/text_render_control.hpp"
#include "src/render/text/text_transform.hpp"
#include "src/text/scaler_context.hpp"

namespace skity {

class ScalerContextEmpty : public ScalerContext {
 protected:
  void GenerateMetrics(GlyphData *) override {}
  void GenerateImage(GlyphData *, const StrokeDesc &) override {}
  bool GeneratePath(GlyphData *) override { return true; }
  void GenerateFontMetrics(FontMetrics *) override {}
  uint16_t OnGetFixedSize() override { return 0; }
};

class ColorfulTypeface : public Typeface {
 public:
  ColorfulTypeface(bool colorful)
      : Typeface(FontStyle()), colorful_(colorful) {}
  ~ColorfulTypeface() override = default;

 public:
  int OnGetTableTags(FontTableTag *tags) const override;
  size_t OnGetTableData(FontTableTag tag, size_t offset, size_t length,
                        void *data) const override;
  void OnCharsToGlyphs(const uint32_t *chars, int count,
                       GlyphID *glyphs) const override;
  std::shared_ptr<Data> OnGetData() override;
  uint32_t OnGetUPEM() const override;
  bool OnContainsColorTable() const override;
  std::unique_ptr<ScalerContext> OnCreateScalerContext(
      const ScalerContextDesc *desc) const override;

  VariationPosition OnGetVariationDesignPosition() const override;
  std::vector<VariationAxis> OnGetVariationDesignParameters() const override;

  std::shared_ptr<Typeface> OnMakeVariation(
      const FontArguments &args) const override;

  void OnGetFontDescriptor(FontDescriptor &desc) const override {}

 private:
  bool colorful_ = false;
};
int ColorfulTypeface::OnGetTableTags(FontTableTag *) const { return 0; }
size_t ColorfulTypeface::OnGetTableData(FontTableTag, size_t, size_t,
                                        void *) const {
  return 0;
}
void ColorfulTypeface::OnCharsToGlyphs(const uint32_t *, int, GlyphID *) const {
}
std::shared_ptr<Data> ColorfulTypeface::OnGetData() { return nullptr; }
uint32_t ColorfulTypeface::OnGetUPEM() const { return 0; }

bool ColorfulTypeface::OnContainsColorTable() const { return colorful_; }

std::unique_ptr<ScalerContext> ColorfulTypeface::OnCreateScalerContext(
    const ScalerContextDesc *) const {
  return std::unique_ptr<ScalerContextEmpty>();
}

VariationPosition ColorfulTypeface::OnGetVariationDesignPosition() const {
  return VariationPosition();
}

std::vector<VariationAxis> ColorfulTypeface::OnGetVariationDesignParameters()
    const {
  return {};
}

std::shared_ptr<Typeface> ColorfulTypeface::OnMakeVariation(
    const FontArguments &args) const {
  return nullptr;
}

}  // namespace skity

TEST(TextRenderControl, disallow_sdf_test) {
  skity::TextRenderControl controller{true};
  skity::Paint paint;

  {
    auto typeface = std::make_shared<skity::ColorfulTypeface>(false);
    EXPECT_TRUE(controller.CanUseDirect(14, skity::Matrix{}, paint, typeface));
    EXPECT_TRUE(controller.CanUseDirect(163, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(256, skity::Matrix{}, paint, typeface));

    EXPECT_FALSE(controller.CanUseSDF(14, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(163, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(256, paint, typeface));
  }

  {
    auto typeface = std::make_shared<skity::ColorfulTypeface>(true);
    EXPECT_TRUE(controller.CanUseDirect(14, skity::Matrix{}, paint, typeface));
    EXPECT_TRUE(controller.CanUseDirect(163, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(256, skity::Matrix{}, paint, typeface));

    EXPECT_FALSE(controller.CanUseSDF(14, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(163, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(256, paint, typeface));
  }
}

TEST(TextRenderControl, allow_sdf_test) {
  skity::TextRenderControl controller{false};
  skity::Paint paint;

  {
    auto typeface = std::make_shared<skity::ColorfulTypeface>(false);
    EXPECT_TRUE(controller.CanUseDirect(14, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(163, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(256, skity::Matrix{}, paint, typeface));

    EXPECT_FALSE(controller.CanUseSDF(14, paint, typeface));
    EXPECT_TRUE(controller.CanUseSDF(163, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(256, paint, typeface));
  }

  {
    auto typeface = std::make_shared<skity::ColorfulTypeface>(true);
    EXPECT_TRUE(controller.CanUseDirect(14, skity::Matrix{}, paint, typeface));
    EXPECT_TRUE(controller.CanUseDirect(163, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(256, skity::Matrix{}, paint, typeface));

    EXPECT_FALSE(controller.CanUseSDF(14, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(163, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(256, paint, typeface));
  }

  paint.SetSDFForSmallText(true);
  {
    auto typeface = std::make_shared<skity::ColorfulTypeface>(false);
    EXPECT_TRUE(controller.CanUseDirect(14, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(controller.CanUseDirect(18, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(163, skity::Matrix{}, paint, typeface));
    EXPECT_FALSE(
        controller.CanUseDirect(256, skity::Matrix{}, paint, typeface));

    EXPECT_FALSE(controller.CanUseSDF(14, paint, typeface));
    EXPECT_TRUE(controller.CanUseSDF(18, paint, typeface));
    EXPECT_TRUE(controller.CanUseSDF(163, paint, typeface));
    EXPECT_FALSE(controller.CanUseSDF(256, paint, typeface));
  }
}

void Another_QRDecompose(const skity::Matrix22 &m, skity::Matrix22 *q,
                         skity::Matrix22 *r) {
  const float &a = m.GetScaleX();
  const float &b = m.GetSkewY();
  float c, s;
  if (0 == b) {
    c = std::copysign(1.f, a);
    s = 0;
  } else if (0 == a) {
    c = 0;
    s = -std::copysign(1.f, b);
  } else if (std::fabs(b) > std::fabs(a)) {
    float t = a / b;
    float u = std::copysign(std::sqrt(1.f + t * t), b);
    s = -1.f / u;
    c = -s * t;
  } else {
    float t = b / a;
    float u = std::copysign(std::sqrt(1.f + t * t), a);
    c = 1.f / u;
    s = -c * t;
  }

  skity::Matrix22 d_q{c, -s, s, c};
  *q = d_q;
  *r = d_q * m;
}

TEST(ScalerContextDesc, decompose_matrix) {
  {
    skity::Matrix22 matrix{2, 0, 2, 0};
    skity::Matrix22 q1, r1;
    matrix.QRDecompose(&q1, &r1);
    skity::Matrix22 q2, r2;
    Another_QRDecompose(matrix, &q2, &r2);
    EXPECT_TRUE(q1.Identity(q2) && r1.Identity(r2));
  }
  {
    skity::Matrix22 matrix{2, 3, 4, 5};
    skity::Matrix22 q1, r1;
    matrix.QRDecompose(&q1, &r1);
    skity::Matrix22 q2, r2;
    Another_QRDecompose(matrix, &q2, &r2);
    EXPECT_TRUE(q1.Identity(q2) && r1.Identity(r2));
  }
  {
    skity::Matrix22 matrix{1, -3, 0, 1};
    skity::Matrix22 q1, r1;
    matrix.QRDecompose(&q1, &r1);
    skity::Matrix22 q2, r2;
    Another_QRDecompose(matrix, &q2, &r2);
    EXPECT_TRUE(q1.Identity(q2) && r1.Identity(r2));
  }
  {
    skity::Matrix22 matrix{1, 1, 1, 1};
    skity::Matrix22 q1, r1;
    matrix.QRDecompose(&q1, &r1);
    skity::Matrix22 q2, r2;
    Another_QRDecompose(matrix, &q2, &r2);
    EXPECT_TRUE(q1.Identity(q2) && r1.Identity(r2));
  }
  {
    skity::Matrix22 matrix{0, 1, 1, 0};
    skity::Matrix22 q1, r1;
    matrix.QRDecompose(&q1, &r1);
    skity::Matrix22 q2, r2;
    Another_QRDecompose(matrix, &q2, &r2);
    EXPECT_TRUE(q1.Identity(q2) && r1.Identity(r2));
  }
  {
    skity::Matrix22 matrix{0, 1, 0, 0};
    skity::Matrix22 q1, r1;
    matrix.QRDecompose(&q1, &r1);
    skity::Matrix22 q2, r2;
    Another_QRDecompose(matrix, &q2, &r2);
    EXPECT_TRUE(q1.Identity(q2) && r1.Identity(r2));
  }
}

static bool dist_equal(uint8_t dist, uint8_t expect) {
  bool equal = dist == expect;
  if (!equal && expect > 0) {
    equal = dist == expect - 1;
  }
  if (!equal && expect < 255) {
    equal = dist == expect + 1;
  }
  return equal;
}

TEST(SdfGen, GenerateSdfImage) {
  {
    size_t width = 2;
    size_t height = 2;
    skity::sdf::Image<uint8_t> src_image(width, height);
    size_t unfiltered_image_size = width * height;
    src_image.Set(0, 0, 255);
    src_image.Set(1, 0, 255);
    src_image.Set(0, 1, 255);
    src_image.Set(1, 1, 255);

    skity::sdf::Image<uint8_t> dst_image =
        skity::sdf::SdfGen::GenerateSdfImage(src_image);

    EXPECT_TRUE(dist_equal(dst_image.Get(4, 4), 151));
    EXPECT_TRUE(dist_equal(dst_image.Get(5, 4), 151));
    EXPECT_TRUE(dist_equal(dst_image.Get(4, 5), 151));
    EXPECT_TRUE(dist_equal(dst_image.Get(5, 5), 151));
  }
  {
    size_t width = 2;
    size_t height = 2;
    skity::sdf::Image<uint8_t> src_image(width, height);
    size_t unfiltered_image_size = width * height;
    src_image.Set(0, 0, 255);
    src_image.Set(1, 0, 255);
    src_image.Set(0, 1, 60);
    src_image.Set(1, 1, 60);

    skity::sdf::Image<uint8_t> dst_image =
        skity::sdf::SdfGen::GenerateSdfImage(src_image);

    EXPECT_TRUE(dist_equal(dst_image.Get(4, 4), 148));
    EXPECT_TRUE(dist_equal(dst_image.Get(5, 4), 148));
    EXPECT_TRUE(dist_equal(dst_image.Get(4, 5), 121));
    EXPECT_TRUE(dist_equal(dst_image.Get(5, 5), 121));
  }
}
