// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_RECORDER_RECORDING_CANVAS_HPP
#define INCLUDE_SKITY_RECORDER_RECORDING_CANVAS_HPP

#include <memory>
#include <skity/graphic/blend_mode.hpp>
#include <skity/macros.hpp>
#include <skity/recorder/display_list.hpp>
#include <skity/render/canvas.hpp>

namespace skity {

struct RecordedOp;
struct DisplayListBuilder;

class SKITY_API RecordingCanvas : public Canvas {
 public:
  RecordingCanvas();
  ~RecordingCanvas() override;

  void BindDisplayListBuilder(DisplayListBuilder* dp_builder);

  RecordedOpOffset GetLastOpOffset() const;

 private:
  template <typename T, typename... Args>
  void Push(Args&&... args);

 protected:
  void OnClipRect(Rect const& rect, ClipOp op) override;
  void OnClipPath(Path const& path, ClipOp op) override;
  void OnDrawRect(Rect const& rect, Paint const& paint) override;
  void OnDrawRRect(RRect const& rrect, Paint const& paint) override;
  void OnDrawPath(Path const& path, Paint const& paint) override;
  void OnSaveLayer(const Rect& bounds, const Paint& paint) override;
  void OnDrawBlob(const TextBlob* blob, float x, float y,
                  Paint const& paint) override;
  void OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                       const Rect& dst, const SamplingOptions& sampling,
                       Paint const* paint) override;
  void OnDrawGlyphs(uint32_t count, const GlyphID glyphs[],
                    const float position_x[], const float position_y[],
                    const Font& font, const Paint& paint) override;
  void OnDrawPaint(Paint const& paint) override;
  void OnSave() override;
  void OnRestore() override;
  void OnRestoreToCount(int saveCount) override;
  void OnTranslate(float dx, float dy) override;
  void OnScale(float sx, float sy) override;
  void OnRotate(float degree) override;
  void OnRotate(float degree, float px, float py) override;
  void OnSkew(float sx, float sy) override;
  void OnConcat(Matrix const& matrix) override;
  void OnSetMatrix(Matrix const& matrix) override;
  void OnResetMatrix() override;
  void OnFlush() override;
  uint32_t OnGetWidth() const override;
  uint32_t OnGetHeight() const override;
  void OnUpdateViewport(uint32_t width, uint32_t height) override;

 private:
  void AccumulateOpBounds(const Rect& raw_bounds, const Paint* paint);

  DisplayListBuilder* dp_builder_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_RECORDER_RECORDING_CANVAS_HPP
