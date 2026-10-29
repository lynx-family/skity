// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_WRAPPER_INCLUDE_SKITY_RENDER_CANVAS_HPP
#define MODULE_CAPI_WRAPPER_INCLUDE_SKITY_RENDER_CANVAS_HPP

#include <skity/c/skity_bitmap.h>
#include <skity/c/skity_canvas.h>

#include <cstdint>
#include <memory>
#include <skity/base.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/text/text.hpp>
#include <skity/types.hpp>

namespace skity {

/** Clip combination operations, mirroring skity::Canvas::ClipOp. */
enum class ClipOp : uint32_t {
  kDifference = SKITY_CLIP_OP_DIFFERENCE,
  kIntersect = SKITY_CLIP_OP_INTERSECT,
};

inline constexpr skity_clip_op to_c(ClipOp op) {
  return static_cast<skity_clip_op>(op);
}

/**
 * @class Canvas
 *
 * Non-owning canvas view. Matches the handle borrowed from
 * Surface::LockCanvas: destroying the view only drops the reference — the
 * surface keeps the underlying object alive. Views are move-only; treat
 * one view per frame — the underlying canvas is replaced on the next
 * Surface::LockCanvas call. The owning form is Canvas::MakeSoftwareCanvas
 * (legacy unique_ptr<Canvas> shape).
 *
 */
class Canvas {
 public:
  Canvas() = default;

  explicit Canvas(skity_canvas canvas) : canvas_(canvas) {}

  Canvas(Canvas&& other) noexcept
      : canvas_(other.canvas_), owning_(other.owning_) {
    other.canvas_ = nullptr;
    other.owning_ = false;
  }
  Canvas& operator=(Canvas&& other) noexcept {
    if (this != &other) {
      Destroy();
      canvas_ = other.canvas_;
      owning_ = other.owning_;
      other.canvas_ = nullptr;
      other.owning_ = false;
    }
    return *this;
  }
  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;

  ~Canvas() { Destroy(); }

  /**
   * Software canvas over a CPU bitmap, the legacy MakeSoftwareCanvas
   * shape: the returned canvas owns its handle (deleted on destruction);
   * the bitmap must outlive it. Null on failure.
   */
  static std::unique_ptr<Canvas> MakeSoftwareCanvas(Bitmap* bitmap);

  skity_canvas get() const { return canvas_; }
  explicit operator bool() const { return canvas_ != nullptr; }

  int Save() { return skity_canvas_save(canvas_); }
  void Restore() { skity_canvas_restore(canvas_); }
  void RestoreToCount(int save_count) {
    skity_canvas_restore_to_count(canvas_, save_count);
  }
  int GetSaveCount() { return skity_canvas_get_save_count(canvas_); }

  void Translate(float dx, float dy) {
    skity_canvas_translate(canvas_, dx, dy);
  }
  void Scale(float sx, float sy) { skity_canvas_scale(canvas_, sx, sy); }
  void Rotate(float degrees) { skity_canvas_rotate(canvas_, degrees); }
  void Rotate(float degrees, float px, float py) {
    skity_canvas_rotate_deg(canvas_, degrees, px, py);
  }
  void Skew(float sx, float sy) { skity_canvas_skew(canvas_, sx, sy); }
  void Concat(const Matrix& matrix) { skity_canvas_concat(canvas_, &matrix); }
  void SetMatrix(const Matrix& matrix) {
    skity_canvas_set_matrix(canvas_, &matrix);
  }
  void ResetMatrix() { skity_canvas_reset_matrix(canvas_); }

  Matrix GetTotalMatrix() const {
    Matrix out;
    skity_canvas_get_total_matrix(canvas_, &out);
    return out;
  }

  void ClipRect(const Rect& rect, ClipOp op = ClipOp::kIntersect) {
    skity_canvas_clip_rect(canvas_, &rect, to_c(op));
  }
  void ClipPath(const Path& path, ClipOp op = ClipOp::kIntersect) {
    skity_canvas_clip_path(canvas_, path.get(), to_c(op));
  }
  void ClipRRect(const Rect& rect, float rx, float ry,
                 ClipOp op = ClipOp::kIntersect) {
    skity_canvas_clip_rrect(canvas_, &rect, rx, ry, to_c(op));
  }

  int SaveLayer(const Rect& bounds, const Paint& paint) {
    return skity_canvas_save_layer(canvas_, &bounds, paint.get());
  }

  void DrawColor(Color color, BlendMode mode = BlendMode::kSrcOver) {
    skity_canvas_draw_color(canvas_, color, to_c(mode));
  }
  void DrawColor(Color4f color, BlendMode mode = BlendMode::kSrcOver) {
    skity_canvas_draw_color4f(canvas_, color, to_c(mode));
  }
  void DrawPaint(const Paint& paint) {
    skity_canvas_draw_paint(canvas_, paint.get());
  }

  void DrawPoint(float x, float y, const Paint& paint) {
    skity_canvas_draw_point(canvas_, x, y, paint.get());
  }
  void DrawLine(float x0, float y0, float x1, float y1, const Paint& paint) {
    skity_canvas_draw_line(canvas_, x0, y0, x1, y1, paint.get());
  }
  void DrawRect(const Rect& rect, const Paint& paint) {
    skity_canvas_draw_rect(canvas_, &rect, paint.get());
  }
  void DrawCircle(float cx, float cy, float radius, const Paint& paint) {
    skity_canvas_draw_circle(canvas_, cx, cy, radius, paint.get());
  }
  void DrawOval(const Rect& oval, const Paint& paint) {
    skity_canvas_draw_oval(canvas_, &oval, paint.get());
  }
  void DrawRoundRect(const Rect& rect, float rx, float ry, const Paint& paint) {
    skity_canvas_draw_round_rect(canvas_, &rect, rx, ry, paint.get());
  }
  /** radii: four (x, y) pairs, order TL, TR, BR, BL. */
  void DrawRRect(const Rect& rect, const Vec2 radii[4], const Paint& paint) {
    skity_canvas_draw_rrect(canvas_, &rect, radii, paint.get());
  }
  /** Rounded-rect value overload (legacy shape). */
  void DrawRRect(const RRect& rrect, const Paint& paint) {
    skity_canvas_draw_rrect(canvas_, &rrect.GetBounds(), rrect.GetRadii(),
                            paint.get());
  }
  void DrawDRRect(const Rect& outer, float outer_rx, float outer_ry,
                  const Rect& inner, float inner_rx, float inner_ry,
                  const Paint& paint) {
    skity_canvas_draw_drrect(canvas_, &outer, outer_rx, outer_ry, &inner,
                             inner_rx, inner_ry, paint.get());
  }
  void DrawArc(const Rect& oval, float start_angle, float sweep_angle,
               bool use_center, const Paint& paint) {
    skity_canvas_draw_arc(canvas_, &oval, start_angle, sweep_angle,
                          use_center ? 1u : 0u, paint.get());
  }
  void DrawPath(const Path& path, const Paint& paint) {
    skity_canvas_draw_path(canvas_, path.get(), paint.get());
  }

  void DrawTextBlob(const std::shared_ptr<TextBlob>& blob, float x, float y,
                    const Paint& paint) {
    skity_canvas_draw_text_blob(canvas_, blob ? blob->get() : nullptr, x, y,
                                paint.get());
  }
  void DrawGlyphs(uint32_t count, const uint16_t* glyphs,
                  const float* positions_x, const float* positions_y,
                  const Font& font, const Paint& paint) {
    skity_canvas_draw_glyphs(canvas_, count, glyphs, positions_x, positions_y,
                             font.get(), paint.get());
  }

  void DrawImage(const std::shared_ptr<Image>& image, float x, float y) {
    skity_canvas_draw_image(canvas_, image ? image->get() : nullptr, x, y);
  }
  void DrawImageWithSampling(const std::shared_ptr<Image>& image, float x,
                             float y, const SamplingOptions& sampling,
                             const Paint& paint) {
    skity_canvas_draw_image_with_sampling(
        canvas_, image ? image->get() : nullptr, x, y, &sampling, paint.get());
  }
  void DrawImageRect(const std::shared_ptr<Image>& image, const Rect& dst,
                     const Paint& paint) {
    skity_canvas_draw_image_to_rect(canvas_, image ? image->get() : nullptr,
                                    &dst, nullptr, paint.get());
  }
  void DrawImageRect(const std::shared_ptr<Image>& image, const Rect& src,
                     const Rect& dst, const Paint& paint) {
    skity_canvas_draw_image_rect(canvas_, image ? image->get() : nullptr, &src,
                                 &dst, nullptr, paint.get());
  }
  void DrawImageRect(const std::shared_ptr<Image>& image, const Rect& src,
                     const Rect& dst, const SamplingOptions& sampling,
                     const Paint& paint) {
    skity_canvas_draw_image_rect(canvas_, image ? image->get() : nullptr, &src,
                                 &dst, &sampling, paint.get());
  }

  /**
   * Per-corner-radii ClipRRect: radii holds 4 Vec2 entries (corner order
   * upper-left, upper-right, lower-right, lower-left).
   */
  void ClipRRect(const Rect& rect, const Vec2 radii[4],
                 ClipOp op = ClipOp::kIntersect) {
    skity_canvas_clip_rrect_radii(canvas_, &rect, radii, to_c(op));
  }

  /** Per-corner-radii DrawDRRect (4 Vec2 entries per radii array). */
  void DrawDRRect(const Rect& outer, const Vec2 outer_radii[4],
                  const Rect& inner, const Vec2 inner_radii[4],
                  const Paint& paint) {
    skity_canvas_draw_drrect_radii(canvas_, &outer, outer_radii, &inner,
                                   inner_radii, paint.get());
  }

  /** Current clip bounds in device (post-transform) coordinates. */
  Rect GetGlobalClipBounds() const {
    Rect out;
    skity_canvas_get_global_clip_bounds(canvas_, &out);
    return out;
  }

  Rect GetLocalClipBounds() const {
    Rect out;
    skity_canvas_get_local_clip_bounds(canvas_, &out);
    return out;
  }
  bool QuickReject(const Rect& rect) const {
    return skity_canvas_quick_reject(canvas_, &rect) != 0;
  }

  uint32_t GetWidth() const { return skity_canvas_get_width(canvas_); }
  uint32_t GetHeight() const { return skity_canvas_get_height(canvas_); }

  /** Submit draw commands to the GPU backend; call before Surface::Flush. */
  void Flush() { skity_canvas_flush(canvas_); }

 protected:
  /** Give up the handle without destroying (ownership transfer helper). */
  skity_canvas Steal() {
    skity_canvas c = canvas_;
    canvas_ = nullptr;
    owning_ = false;
    return c;
  }

 private:
  explicit Canvas(skity_canvas canvas, bool owning)
      : canvas_(canvas), owning_(owning) {}

  void Destroy() {
    if (owning_ && canvas_ != nullptr) {
      skity_canvas_destroy(canvas_);
    }
    canvas_ = nullptr;
    owning_ = false;
  }

  skity_canvas canvas_ = nullptr;
  bool owning_ = false;
};

inline std::unique_ptr<Canvas> Canvas::MakeSoftwareCanvas(Bitmap* bitmap) {
  if (bitmap == nullptr || !*bitmap) {
    return nullptr;
  }
  skity_canvas handle = skity_canvas_make_software_canvas(bitmap->get());
  if (handle == nullptr) {
    return nullptr;
  }
  return std::unique_ptr<Canvas>(new Canvas(handle, true));
}

}  // namespace skity

#endif  // MODULE_CAPI_WRAPPER_INCLUDE_SKITY_RENDER_CANVAS_HPP
