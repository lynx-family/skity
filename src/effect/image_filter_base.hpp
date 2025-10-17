// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_EFFECT_IMAGE_FILTER_BASE_HPP
#define SRC_EFFECT_IMAGE_FILTER_BASE_HPP

#include <memory>
#include <skity/effect/image_filter.hpp>
#include <vector>

namespace skity {

class Bitmap;
class Canvas;
class DropShadowImageFilter;
class Rect;
class Paint;
class Path;

enum class ImageFilterType {
  kIdentity = 0,
  kBlur = 1,
  kDropShadow = 2,
  kDilate = 3,
  kErode = 4,
  kMatix = 5,
  kColorFilter = 6,
  kCompose = 7,
};

constexpr static float kBlueSigmaScale = 0.57735f;

// Alternative -- skia SigmaRadius, but donot use with ConvertRadiusToSigma
inline float ConvertSigmaToRadius(float sigma) {
  return sigma > 0.5f ? (sigma - 0.5f) / kBlueSigmaScale : 0.0f;
}

inline float ConvertRadiusToSigma(float radius) {
  return radius > 0 ? kBlueSigmaScale * radius + 0.5f : 0.0f;
}

class ImageFilterBase : public ImageFilter {
 public:
  static Rect ApproximateFilteredBounds(const Rect& src, float radius_x,
                                        float radius_y);
#if defined(SKITY_CPU)
  static void BlurBitmapToCanvas(Canvas* canvas, Bitmap& bitmap,
                                 const Rect& filter_bounds, const Paint& paint,
                                 float radius_x, float radius_y);
  virtual void OnFilter(Canvas*, Bitmap&, const Rect&, const Paint&) const {}
#endif
  ~ImageFilterBase() override = default;

  virtual float GetRadiusX() const { return 0.f; }
  virtual float GetRadiusY() const { return 0.f; }

  virtual float GetOffsetX() const { return 0.f; }
  virtual float GetOffsetY() const { return 0.f; }
  virtual Color GetColor() const { return Color_TRANSPARENT; }

  virtual ImageFilterType GetType() const = 0;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

 protected:
  explicit ImageFilterBase(std::vector<ImageFilter*> inputs)
      : inputs_(std::move(inputs)) {}

 private:
  std::vector<ImageFilter*> inputs_;
};

static inline ImageFilterBase* As_IFB(ImageFilter* filter) {
  return static_cast<ImageFilterBase*>(filter);
}

static inline const ImageFilterBase* As_IFB(const ImageFilter* filter) {
  return static_cast<const ImageFilterBase*>(filter);
}

bool operator==(const ImageFilterBase& a, const ImageFilterBase& b);

class BlurImageFilter : public ImageFilterBase {
 public:
  BlurImageFilter(float sigma_x, float sigma_y)
      : ImageFilterBase({nullptr}),
        radius_x_(ConvertSigmaToRadius(sigma_x)),
        radius_y_(ConvertSigmaToRadius(sigma_y)) {}
  float GetRadiusX() const override { return radius_x_; }
  float GetRadiusY() const override { return radius_y_; }
#if defined(SKITY_CPU)
  void OnFilter(Canvas* canvas, Bitmap& bitmap, const Rect& filter_bounds,
                const Paint& paint) const override;
#endif

  ImageFilterType GetType() const override { return ImageFilterType::kBlur; }

  Rect ComputeFastBounds(const Rect& src) const override {
    return Rect::MakeLTRB(src.Left() - radius_x_,   //
                          src.Top() - radius_y_,    //
                          src.Right() + radius_x_,  //
                          src.Bottom() + radius_y_);
  }

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

 private:
  float radius_x_;
  float radius_y_;
};

class DropShadowImageFilter : public ImageFilterBase {
 public:
  DropShadowImageFilter(float dx, float dy, float sigma_x, float sigma_y,
                        Color color, std::shared_ptr<ImageFilter> input,
                        const Rect& crop_rect)
      : ImageFilterBase({input.get()}),
        dx_(dx),
        dy_(dy),
        radius_x_(ConvertSigmaToRadius(sigma_x)),
        radius_y_(ConvertSigmaToRadius(sigma_y)),
        color_(color),
        crop_rect_(crop_rect) {}
  float GetRadiusX() const override { return radius_x_; }
  float GetRadiusY() const override { return radius_y_; }
  float GetOffsetX() const override { return dx_; }
  float GetOffsetY() const override { return dy_; }
  Color GetColor() const override { return color_; }
#if defined(SKITY_CPU)
  void OnFilter(Canvas* canvas, Bitmap& bitmap, const Rect& filter_bounds,
                const Paint& paint) const override;
#endif

  ImageFilterType GetType() const override {
    return ImageFilterType::kDropShadow;
  };

  Rect ComputeFastBounds(const Rect& src) const override {
    auto rect = Rect::MakeLTRB(src.Left() - radius_x_,   //
                               src.Top() - radius_y_,    //
                               src.Right() + radius_x_,  //
                               src.Bottom() + radius_y_);
    rect.Offset(dx_, dy_);
    rect.Join(src);
    return rect;
  }

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

 private:
  float dx_;
  float dy_;
  float radius_x_;
  float radius_y_;
  Color color_;
  const Rect crop_rect_;
};

class MorphologyImageFilter : public ImageFilterBase {
 public:
  MorphologyImageFilter(ImageFilterType type, float radius_x, float radius_y)
      : ImageFilterBase({nullptr}),
        radius_x_(radius_x),
        radius_y_(radius_y),
        type_(type) {}
  float GetRadiusX() const override { return radius_x_; }
  float GetRadiusY() const override { return radius_y_; }
#if defined(SKITY_CPU)
  void OnFilter(Canvas* canvas, Bitmap& bitmap, const Rect& filter_bounds,
                const Paint& paint) const override;
#endif

  /**
   * All morphology procs have the same signature: src is the source buffer, dst
   * the destination buffer, radius is the morphology radius, width and height
   * are the bounds of the destination buffer (in pixels), and srcStride and
   * dstStride are the number of pixels per row in each buffer. All buffers are
   * 8888.
   */

  typedef void (*Proc)(const PMColor* src, PMColor* dst, int radius, int width,
                       int height, int srcStride, int dstStride);

  ImageFilterType GetType() const override { return type_; }

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

 private:
  float radius_x_;
  float radius_y_;
  ImageFilterType type_;
};

class MatrixImageFilter : public ImageFilterBase {
 public:
  explicit MatrixImageFilter(const Matrix& matrix);

  MatrixImageFilter(ImageFilter* input, const Matrix& matrix);

  const Matrix& GetMatrix() const { return matrix_; }

  ImageFilterType GetType() const override { return ImageFilterType::kMatix; }

  Rect ComputeFastBounds(const Rect& src) const override {
    Rect result;
    if (matrix_.MapRect(&result, src)) {
      return result;
    }
    return src;
  }

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

 private:
  Matrix matrix_;
};

class ColorFilterImageFilter : public ImageFilterBase {
 public:
  explicit ColorFilterImageFilter(std::shared_ptr<ColorFilter> cf);

  ColorFilterImageFilter(ImageFilter* input, std::shared_ptr<ColorFilter> cf);

  ImageFilterType GetType() const override {
    return ImageFilterType::kColorFilter;
  }

  std::shared_ptr<ColorFilter> GetColorFilter() const { return color_filter_; }

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

 private:
  std::shared_ptr<ColorFilter> color_filter_;
};

class ComposeImageFilter : public ImageFilterBase {
 public:
  ComposeImageFilter(std::shared_ptr<ImageFilter> outer,
                     std::shared_ptr<ImageFilter> inner)
      : ImageFilterBase({outer.get(), inner.get()}),
        outer_(outer),
        inner_(inner) {}

  ImageFilterType GetType() const override {
    return ImageFilterType::kCompose;
  };

  std::shared_ptr<ImageFilter> GetOuter() const { return outer_; }

  std::shared_ptr<ImageFilter> GetInner() const { return inner_; }

  Rect ComputeFastBounds(const Rect& src) const override {
    if (!inner_ && !outer_) {
      return src;
    }

    if (!inner_) {
      return outer_->ComputeFastBounds(src);
    }

    if (!outer_) {
      return inner_->ComputeFastBounds(src);
    }

    return outer_->ComputeFastBounds(inner_->ComputeFastBounds(src));
  }

  std::string_view ProcName() const override { return "SkComposeImageFilter"; }

 private:
  std::shared_ptr<ImageFilter> outer_;
  std::shared_ptr<ImageFilter> inner_;
};

}  // namespace skity
#endif  // SRC_EFFECT_IMAGE_FILTER_BASE_HPP
