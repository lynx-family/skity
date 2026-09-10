// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CANVAS_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CANVAS_HPP

// Canvas wrappers of the header-only RAII layer: a non-owning Canvas view
// (matching the handle borrowed from skity_surface_lock_canvas) and the
// owning SoftwareCanvas over a CPU bitmap. See skity.hpp for the layer's
// design.

#include <skity_c/skity_bitmap.h>
#include <skity_c/skity_canvas.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_paint.hpp>
#include <skity_hpp/skity_path.hpp>
#include <skity_hpp/skity_text.hpp>
#include <skity_hpp/skity_types.hpp>

namespace skity {
namespace raii {

/** Clip combination operations, mirroring skity::Canvas::ClipOp. */
enum class ClipOp : uint32_t {
  kDifference = SKITY_CLIP_OP_DIFFERENCE,
  kIntersect = SKITY_CLIP_OP_INTERSECT,
};

inline constexpr skity_clip_op to_c(ClipOp op) {
  return static_cast<skity_clip_op>(op);
}

/**
 * Non-owning canvas view. Matches the handle borrowed from
 * Surface::LockCanvas: destroying the view only drops the reference — the
 * surface (or a SoftwareCanvas) keeps the underlying object alive.
 * Cheap to copy; treat one view per frame (see the frame-boundary notes in
 * lat.md/canvas).
 */
class Canvas {
 public:
  Canvas() = default;

  explicit Canvas(skity_canvas canvas) : canvas_(canvas) {}

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
  /** @p radii: four (x, y) pairs, order TL, TR, BR, BL. */
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

  void DrawTextBlob(const TextBlob& blob, float x, float y,
                    const Paint& paint) {
    skity_canvas_draw_text_blob(canvas_, blob.get(), x, y, paint.get());
  }
  void DrawGlyphs(uint32_t count, const uint16_t* glyphs,
                  const float* positions_x, const float* positions_y,
                  const Font& font, const Paint& paint) {
    skity_canvas_draw_glyphs(canvas_, count, glyphs, positions_x, positions_y,
                             font.get(), paint.get());
  }

  void DrawImage(skity_image image, float x, float y) {
    skity_canvas_draw_image(canvas_, image, x, y);
  }
  void DrawImageRect(skity_image image, const Rect& dst, const Paint& paint) {
    skity_canvas_draw_image_to_rect(canvas_, image, &dst, nullptr, paint.get());
  }
  void DrawImageRect(skity_image image, const Rect& src, const Rect& dst,
                     const Paint& paint) {
    skity_canvas_draw_image_rect(canvas_, image, &src, &dst, nullptr,
                                 paint.get());
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

 private:
  skity_canvas canvas_ = nullptr;
};

/**
 * Owning canvas over a CPU bitmap (skity_canvas_make_software_canvas).
 * Implicitly converts to the non-owning Canvas view for drawing calls.
 * The bitmap must outlive the canvas.
 */
class SoftwareCanvas
    : public detail::OwnHandle<skity_canvas, skity_canvas_destroy> {
 public:
  explicit SoftwareCanvas(skity_bitmap bitmap)
      : OwnHandle(skity_canvas_make_software_canvas(bitmap)) {}

  operator Canvas() const { return Canvas(get()); }
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_CANVAS_HPP
