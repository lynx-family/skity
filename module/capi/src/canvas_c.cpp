// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/c/skity_canvas.h>

#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/geometry/rrect.hpp>
#include <skity/geometry/vector.hpp>
#include <skity/graphic/bitmap.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/font.hpp>
#include <skity/text/text_blob.hpp>

#include "handle.hpp"

namespace {

skity::Canvas* canvas_of(skity_canvas handle) {
  auto* w =
      skity::capi::resolve<skity_canvas_s>(handle, SKITY_OBJECT_TYPE_CANVAS);
  return w ? static_cast<skity::Canvas*>(w->impl.get()) : nullptr;
}

skity::Paint* paint_of(skity_paint handle) {
  auto* w =
      skity::capi::resolve<skity_paint_s>(handle, SKITY_OBJECT_TYPE_PAINT);
  return w ? static_cast<skity::Paint*>(w->impl.get()) : nullptr;
}

skity::Path* path_of(skity_path handle) {
  auto* w = skity::capi::resolve<skity_path_s>(handle, SKITY_OBJECT_TYPE_PATH);
  return w ? static_cast<skity::Path*>(w->impl.get()) : nullptr;
}

skity::Canvas::ClipOp to_clip_op(skity_clip_op op) {
  return op == SKITY_CLIP_OP_DIFFERENCE ? skity::Canvas::ClipOp::kDifference
                                        : skity::Canvas::ClipOp::kIntersect;
}

skity::SamplingOptions to_sampling_options(
    const skity_sampling_options* sampling) {
  skity::SamplingOptions so{};
  if (sampling != nullptr) {
    so.filter = static_cast<skity::FilterMode>(sampling->filter);
    so.mipmap = static_cast<skity::MipmapMode>(sampling->mipmap);
    so.cubic.B = sampling->cubic_b;
    so.cubic.C = sampling->cubic_c;
  }
  return so;
}

skity_clip_op from_clip_op(skity::Canvas::ClipOp op) {
  return op == skity::Canvas::ClipOp::kDifference ? SKITY_CLIP_OP_DIFFERENCE
                                                  : SKITY_CLIP_OP_INTERSECT;
}

/*
 * Borrowed-handle constructors for the objects a delegate callback receives.
 * Each wrapper holds a non-owning view (empty deleter), so the handle is
 * valid only while the callback runs — the documented delegate contract.
 */
skity_paint borrow_paint(const skity::Paint& paint) {
  std::shared_ptr<skity::Paint> impl(const_cast<skity::Paint*>(&paint),
                                     [](skity::Paint*) {});
  return skity::capi::alloc_handle<skity_paint_s>(SKITY_OBJECT_TYPE_PAINT, 0,
                                                  std::move(impl));
}

skity_path borrow_path(const skity::Path& path) {
  std::shared_ptr<skity::Path> impl(const_cast<skity::Path*>(&path),
                                    [](skity::Path*) {});
  return skity::capi::alloc_handle<skity_path_s>(SKITY_OBJECT_TYPE_PATH, 0,
                                                 std::move(impl));
}

skity_text_blob borrow_blob(const skity::TextBlob* blob) {
  std::shared_ptr<skity::TextBlob> impl(const_cast<skity::TextBlob*>(blob),
                                        [](skity::TextBlob*) {});
  return skity::capi::alloc_handle<skity_text_blob_s>(
      SKITY_OBJECT_TYPE_TEXT_BLOB, 0, std::move(impl));
}

skity_font borrow_font(const skity::Font& font) {
  std::shared_ptr<skity::Font> impl(const_cast<skity::Font*>(&font),
                                    [](skity::Font*) {});
  return skity::capi::alloc_handle<skity_font_s>(SKITY_OBJECT_TYPE_FONT, 0,
                                                 std::move(impl));
}

/*
 * Canvas that forwards the recording-relevant On* virtuals to a C callback
 * table. Ops the table does not model (rounded-rect / line / circle / oval /
 * arc drawing, rounded-rect clipping) keep the Canvas base implementations,
 * which convert them to paths and re-enter through OnDrawPath / OnClipPath —
 * the same op granularity a serialized recording captures.
 */
class DelegateCanvas final : public skity::Canvas {
 public:
  DelegateCanvas(const skity_canvas_delegate_callbacks& callbacks,
                 void* userdata)
      : Canvas(), callbacks_(callbacks), userdata_(userdata) {}

  ~DelegateCanvas() override {
    if (callbacks_.on_destroy != nullptr) {
      callbacks_.on_destroy(userdata_);
    }
  }

 protected:
  void OnSave() override { Call(callbacks_.on_save); }

  void OnRestore() override { Call(callbacks_.on_restore); }

  // The base RestoreToCount already loops Restore() (each firing on_restore),
  // so no separate callback exists; the override only satisfies the vtable.
  void OnRestoreToCount(int) override {}

  void OnSaveLayer(const skity::Rect& bounds,
                   const skity::Paint& paint) override {
    Call(callbacks_.on_save_layer, reinterpret_cast<const skity_rect*>(&bounds),
         borrow_paint(paint));
  }

  void OnTranslate(float dx, float dy) override {
    Call(callbacks_.on_translate, dx, dy);
  }

  void OnScale(float sx, float sy) override {
    Call(callbacks_.on_scale, sx, sy);
  }

  void OnRotate(float degrees) override { Call(callbacks_.on_rotate, degrees); }

  void OnRotate(float degrees, float px, float py) override {
    Call(callbacks_.on_rotate_point, degrees, px, py);
  }

  void OnSkew(float sx, float sy) override { Call(callbacks_.on_skew, sx, sy); }

  void OnConcat(const skity::Matrix& matrix) override {
    Call(callbacks_.on_concat, reinterpret_cast<const skity_matrix*>(&matrix));
  }

  void OnSetMatrix(const skity::Matrix& matrix) override {
    Call(callbacks_.on_set_matrix,
         reinterpret_cast<const skity_matrix*>(&matrix));
  }

  void OnResetMatrix() override { Call(callbacks_.on_reset_matrix); }

  void OnClipRect(const skity::Rect& rect, skity::Canvas::ClipOp op) override {
    Call(callbacks_.on_clip_rect, reinterpret_cast<const skity_rect*>(&rect),
         from_clip_op(op));
  }

  void OnClipPath(const skity::Path& path, skity::Canvas::ClipOp op) override {
    Call(callbacks_.on_clip_path, borrow_path(path), from_clip_op(op));
  }

  void OnDrawPaint(const skity::Paint& paint) override {
    Call(callbacks_.on_draw_paint, borrow_paint(paint));
  }

  void OnDrawRect(const skity::Rect& rect, const skity::Paint& paint) override {
    Call(callbacks_.on_draw_rect, reinterpret_cast<const skity_rect*>(&rect),
         borrow_paint(paint));
  }

  void OnDrawPath(const skity::Path& path, const skity::Paint& paint) override {
    Call(callbacks_.on_draw_path, borrow_path(path), borrow_paint(paint));
  }

  void OnDrawBlob(const skity::TextBlob* blob, float x, float y,
                  const skity::Paint& paint) override {
    Call(callbacks_.on_draw_blob, borrow_blob(blob), x, y, borrow_paint(paint));
  }

  void OnDrawImageRect(std::shared_ptr<skity::Image> image,
                       const skity::Rect& src, const skity::Rect& dst,
                       const skity::SamplingOptions& sampling,
                       const skity::Paint* paint) override {
    // The image arrives as a real reference; the wrapper owns it for the
    // callback duration and drops the reference on handle release.
    skity_image image_handle = skity::capi::alloc_handle<skity_image_s>(
        SKITY_OBJECT_TYPE_IMAGE, SKITY_HANDLE_OWNING, std::move(image));
    Call(callbacks_.on_draw_image_rect, image_handle,
         reinterpret_cast<const skity_rect*>(&src),
         reinterpret_cast<const skity_rect*>(&dst),
         reinterpret_cast<const skity_sampling_options*>(&sampling),
         paint != nullptr ? borrow_paint(*paint) : nullptr);
  }

  void OnDrawGlyphs(uint32_t count, const skity::GlyphID glyphs[],
                    const float position_x[], const float position_y[],
                    const skity::Font& font,
                    const skity::Paint& paint) override {
    Call(callbacks_.on_draw_glyphs, count,
         reinterpret_cast<const uint16_t*>(glyphs), position_x, position_y,
         borrow_font(font), borrow_paint(paint));
  }

  // Nothing rasterizes through a delegate canvas; flush is a no-op, matching
  // the recording canvases this delegates for.
  void OnFlush() override {}

  uint32_t OnGetWidth() const override { return 0u; }

  uint32_t OnGetHeight() const override { return 0u; }

 private:
  template <typename Fn, typename... Args>
  void Call(Fn fn, Args&&... args) {
    if (fn != nullptr) {
      fn(userdata_, std::forward<Args>(args)...);
    }
  }

  skity_canvas_delegate_callbacks callbacks_;
  void* userdata_;
};

}  // namespace

extern "C" {

int32_t skity_canvas_save(skity_canvas canvas) {
  auto* c = canvas_of(canvas);
  return c ? c->Save() : 0;
}

void skity_canvas_restore(skity_canvas canvas) {
  if (auto* c = canvas_of(canvas)) c->Restore();
}

void skity_canvas_restore_to_count(skity_canvas canvas, int32_t save_count) {
  if (auto* c = canvas_of(canvas)) c->RestoreToCount(save_count);
}

int32_t skity_canvas_get_save_count(skity_canvas canvas) {
  auto* c = canvas_of(canvas);
  return c ? c->GetSaveCount() : 0;
}

void skity_canvas_translate(skity_canvas canvas, float dx, float dy) {
  if (auto* c = canvas_of(canvas)) c->Translate(dx, dy);
}

void skity_canvas_scale(skity_canvas canvas, float sx, float sy) {
  if (auto* c = canvas_of(canvas)) c->Scale(sx, sy);
}

void skity_canvas_rotate(skity_canvas canvas, float degrees) {
  if (auto* c = canvas_of(canvas)) c->Rotate(degrees);
}

void skity_canvas_rotate_deg(skity_canvas canvas, float degrees, float px,
                             float py) {
  if (auto* c = canvas_of(canvas)) c->Rotate(degrees, px, py);
}

void skity_canvas_skew(skity_canvas canvas, float sx, float sy) {
  if (auto* c = canvas_of(canvas)) c->Skew(sx, sy);
}

void skity_canvas_concat(skity_canvas canvas, const skity_matrix* matrix) {
  auto* c = canvas_of(canvas);
  if (c != nullptr && matrix != nullptr) {
    c->Concat(*reinterpret_cast<const skity::Matrix*>(matrix));
  }
}

void skity_canvas_set_matrix(skity_canvas canvas, const skity_matrix* matrix) {
  auto* c = canvas_of(canvas);
  if (c != nullptr && matrix != nullptr) {
    c->SetMatrix(*reinterpret_cast<const skity::Matrix*>(matrix));
  }
}

void skity_canvas_reset_matrix(skity_canvas canvas) {
  if (auto* c = canvas_of(canvas)) c->ResetMatrix();
}

void skity_canvas_clip_rect(skity_canvas canvas, const skity_rect* rect,
                            skity_clip_op op) {
  auto* c = canvas_of(canvas);
  if (c != nullptr && rect != nullptr) {
    c->ClipRect(*reinterpret_cast<const skity::Rect*>(rect), to_clip_op(op));
  }
}

void skity_canvas_clip_path(skity_canvas canvas, skity_path path,
                            skity_clip_op op) {
  auto* c = canvas_of(canvas);
  auto* p = path_of(path);
  if (c != nullptr && p != nullptr) {
    c->ClipPath(*p, to_clip_op(op));
  }
}

void skity_canvas_draw_color(skity_canvas canvas, skity_color color,
                             skity_blend_mode mode) {
  if (auto* c = canvas_of(canvas)) {
    c->DrawColor(static_cast<skity::Color>(color),
                 static_cast<skity::BlendMode>(mode));
  }
}

void skity_canvas_draw_color4f(skity_canvas canvas, skity_color4f color,
                               skity_blend_mode mode) {
  if (auto* c = canvas_of(canvas)) {
    c->DrawColor(*reinterpret_cast<const skity::Color4f*>(&color),
                 static_cast<skity::BlendMode>(mode));
  }
}

void skity_canvas_draw_paint(skity_canvas canvas, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && p != nullptr) c->DrawPaint(*p);
}

void skity_canvas_draw_point(skity_canvas canvas, float x, float y,
                             skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && p != nullptr) c->DrawCircle(x, y, 0, *p);
}

void skity_canvas_draw_line(skity_canvas canvas, float x0, float y0, float x1,
                            float y1, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && p != nullptr) c->DrawLine(x0, y0, x1, y1, *p);
}

void skity_canvas_draw_rect(skity_canvas canvas, const skity_rect* rect,
                            skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && rect != nullptr && p != nullptr) {
    c->DrawRect(*reinterpret_cast<const skity::Rect*>(rect), *p);
  }
}

void skity_canvas_draw_circle(skity_canvas canvas, float cx, float cy,
                              float radius, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && p != nullptr) c->DrawCircle(cx, cy, radius, *p);
}

void skity_canvas_draw_oval(skity_canvas canvas, const skity_rect* oval,
                            skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && oval != nullptr && p != nullptr) {
    c->DrawOval(*reinterpret_cast<const skity::Rect*>(oval), *p);
  }
}

void skity_canvas_draw_round_rect(skity_canvas canvas, const skity_rect* rect,
                                  float rx, float ry, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && rect != nullptr && p != nullptr) {
    c->DrawRoundRect(*reinterpret_cast<const skity::Rect*>(rect), rx, ry, *p);
  }
}

void skity_canvas_draw_rrect(skity_canvas canvas, const skity_rect* rect,
                             const skity_vec2* radii, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c == nullptr || rect == nullptr || radii == nullptr || p == nullptr) {
    return;
  }
  skity::RRect rr;
  rr.SetRectRadii(*reinterpret_cast<const skity::Rect*>(rect),
                  reinterpret_cast<const skity::Vec2*>(radii));
  c->DrawRRect(rr, *p);
}

void skity_canvas_draw_path(skity_canvas canvas, skity_path path,
                            skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = path_of(path);
  auto* pt = paint_of(paint);
  if (c != nullptr && p != nullptr && pt != nullptr) {
    c->DrawPath(*p, *pt);
  }
}

void skity_canvas_draw_image(skity_canvas canvas, skity_image image, float x,
                             float y) {
  auto* c = canvas_of(canvas);
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  if (c != nullptr && img != nullptr) {
    c->DrawImage(img, x, y);
  }
}

void skity_canvas_draw_image_with_sampling(
    skity_canvas canvas, skity_image image, float x, float y,
    const skity_sampling_options* sampling, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  if (c == nullptr || img == nullptr) {
    return;
  }
  c->DrawImage(img, x, y, to_sampling_options(sampling), paint_of(paint));
}

void skity_canvas_draw_image_to_rect(skity_canvas canvas, skity_image image,
                                     const skity_rect* dst,
                                     const skity_sampling_options* sampling,
                                     skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  if (c == nullptr || img == nullptr || dst == nullptr) {
    return;
  }
  c->DrawImage(img, *reinterpret_cast<const skity::Rect*>(dst),
               to_sampling_options(sampling), paint_of(paint));
}

void skity_canvas_draw_image_rect(skity_canvas canvas, skity_image image,
                                  const skity_rect* src, const skity_rect* dst,
                                  const skity_sampling_options* sampling,
                                  skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto img = skity::capi::get_impl<skity_image_s, skity::Image>(
      image, SKITY_OBJECT_TYPE_IMAGE);
  auto* p = paint_of(paint);
  if (c == nullptr || img == nullptr || src == nullptr || dst == nullptr) {
    return;
  }
  skity::SamplingOptions so{};
  if (sampling != nullptr) {
    so.filter = static_cast<skity::FilterMode>(sampling->filter);
    so.mipmap = static_cast<skity::MipmapMode>(sampling->mipmap);
    so.cubic.B = sampling->cubic_b;
    so.cubic.C = sampling->cubic_c;
  }
  c->DrawImageRect(img, *reinterpret_cast<const skity::Rect*>(src),
                   *reinterpret_cast<const skity::Rect*>(dst), so, p);
}

void skity_canvas_draw_text_blob(skity_canvas canvas, skity_text_blob blob,
                                 float x, float y, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto b = skity::capi::get_impl<skity_text_blob_s>(
      blob, SKITY_OBJECT_TYPE_TEXT_BLOB);
  auto* p = paint_of(paint);
  if (c != nullptr && b != nullptr && p != nullptr) {
    c->DrawTextBlob(b, x, y, *p);
  }
}

skity_vec2 skity_canvas_simple_text_bounds(skity_canvas canvas,
                                           const char* text,
                                           skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c == nullptr || p == nullptr || text == nullptr) {
    return skity_vec2{{{0.f, 0.f}}};
  }
  skity::Vec2 bounds = c->SimpleTextBounds(text, *p);
  return skity_vec2{{{bounds.x, bounds.y}}};
}

void skity_canvas_clip_rrect(skity_canvas canvas, const skity_rect* rect,
                             float rx, float ry, skity_clip_op op) {
  auto* c = canvas_of(canvas);
  if (c == nullptr || rect == nullptr) {
    return;
  }
  auto rr = skity::RRect::MakeRectXY(
      *reinterpret_cast<const skity::Rect*>(rect), rx, ry);
  c->ClipRRect(rr, to_clip_op(op));
}

int32_t skity_canvas_save_layer(skity_canvas canvas, const skity_rect* bounds,
                                skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c == nullptr || p == nullptr) {
    return 0;
  }
  skity::Rect b = bounds != nullptr
                      ? *reinterpret_cast<const skity::Rect*>(bounds)
                      : skity::Rect{};
  return c->SaveLayer(b, *p);
}

void skity_canvas_draw_arc(skity_canvas canvas, const skity_rect* oval,
                           float start_angle, float sweep_angle,
                           uint32_t use_center, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c != nullptr && oval != nullptr && p != nullptr) {
    c->DrawArc(*reinterpret_cast<const skity::Rect*>(oval), start_angle,
               sweep_angle, use_center != 0, *p);
  }
}

void skity_canvas_draw_drrect(skity_canvas canvas, const skity_rect* outer,
                              float outer_rx, float outer_ry,
                              const skity_rect* inner, float inner_rx,
                              float inner_ry, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c == nullptr || outer == nullptr || inner == nullptr || p == nullptr) {
    return;
  }
  auto o = skity::RRect::MakeRectXY(
      *reinterpret_cast<const skity::Rect*>(outer), outer_rx, outer_ry);
  auto i = skity::RRect::MakeRectXY(
      *reinterpret_cast<const skity::Rect*>(inner), inner_rx, inner_ry);
  c->DrawDRRect(o, i, *p);
}

void skity_canvas_get_total_matrix(skity_canvas canvas, skity_matrix* out) {
  auto* c = canvas_of(canvas);
  if (c == nullptr || out == nullptr) {
    return;
  }
  *reinterpret_cast<skity::Matrix*>(out) = c->GetTotalMatrix();
}

void skity_canvas_get_local_clip_bounds(skity_canvas canvas, skity_rect* out) {
  auto* c = canvas_of(canvas);
  if (c == nullptr || out == nullptr) {
    return;
  }
  skity::Rect b = c->GetLocalClipBounds();
  out->left = b.Left();
  out->top = b.Top();
  out->right = b.Right();
  out->bottom = b.Bottom();
}

uint32_t skity_canvas_quick_reject(skity_canvas canvas,
                                   const skity_rect* rect) {
  auto* c = canvas_of(canvas);
  if (c == nullptr || rect == nullptr) {
    return 1;
  }
  return c->QuickReject(*reinterpret_cast<const skity::Rect*>(rect)) ? 1 : 0;
}

void skity_canvas_draw_glyphs(skity_canvas canvas, uint32_t count,
                              const uint16_t* glyphs, const float* positions_x,
                              const float* positions_y, skity_font font,
                              skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto f = skity::capi::get_impl<skity_font_s, skity::Font>(
      font, SKITY_OBJECT_TYPE_FONT);
  auto* p = paint_of(paint);
  if (c == nullptr || f == nullptr || p == nullptr || glyphs == nullptr ||
      positions_x == nullptr || positions_y == nullptr) {
    return;
  }
  c->DrawGlyphs((int)count, glyphs, positions_x, positions_y, *f, *p);
}

void skity_canvas_flush(skity_canvas canvas) {
  if (auto* c = canvas_of(canvas)) c->Flush();
}

uint32_t skity_canvas_get_width(skity_canvas canvas) {
  auto* c = canvas_of(canvas);
  return c ? c->Width() : 0;
}

uint32_t skity_canvas_get_height(skity_canvas canvas) {
  auto* c = canvas_of(canvas);
  return c ? c->Height() : 0;
}

skity_canvas skity_canvas_make_software_canvas(skity_bitmap bitmap) {
  auto b = skity::capi::get_impl<skity_bitmap_s, skity::Bitmap>(
      bitmap, SKITY_OBJECT_TYPE_BITMAP);
  if (b == nullptr) {
    return nullptr;
  }
  std::unique_ptr<skity::Canvas> canvas =
      skity::Canvas::MakeSoftwareCanvas(b.get());
  if (canvas == nullptr) {
    return nullptr;
  }
  std::shared_ptr<skity::Canvas> impl(canvas.release());
  return skity::capi::alloc_handle<skity_canvas_s>(
      SKITY_OBJECT_TYPE_CANVAS, SKITY_HANDLE_OWNING, std::move(impl));
}

skity_canvas skity_canvas_create_delegate(
    const skity_canvas_delegate_callbacks* callbacks, void* userdata) {
  if (callbacks == nullptr) {
    return nullptr;
  }
  return skity::capi::alloc_handle<skity_canvas_s>(
      SKITY_OBJECT_TYPE_CANVAS, SKITY_HANDLE_OWNING,
      std::make_shared<DelegateCanvas>(*callbacks, userdata));
}

void skity_canvas_destroy(skity_canvas canvas) {
  skity::capi::destroy_handle<skity_canvas_s>(canvas, SKITY_OBJECT_TYPE_CANVAS);
}

void skity_canvas_clip_rrect_radii(skity_canvas canvas, const skity_rect* rect,
                                   const skity_vec2* radii, skity_clip_op op) {
  auto* c = canvas_of(canvas);
  if (c == nullptr || rect == nullptr || radii == nullptr) {
    return;
  }
  skity::RRect rr;
  rr.SetRectRadii(*reinterpret_cast<const skity::Rect*>(rect),
                  reinterpret_cast<const skity::Vec2*>(radii));
  c->ClipRRect(rr, to_clip_op(op));
}

void skity_canvas_draw_drrect_radii(
    skity_canvas canvas, const skity_rect* outer, const skity_vec2* outer_radii,
    const skity_rect* inner, const skity_vec2* inner_radii, skity_paint paint) {
  auto* c = canvas_of(canvas);
  auto* p = paint_of(paint);
  if (c == nullptr || p == nullptr || outer == nullptr ||
      outer_radii == nullptr || inner == nullptr || inner_radii == nullptr) {
    return;
  }
  skity::RRect o;
  o.SetRectRadii(*reinterpret_cast<const skity::Rect*>(outer),
                 reinterpret_cast<const skity::Vec2*>(outer_radii));
  skity::RRect i;
  i.SetRectRadii(*reinterpret_cast<const skity::Rect*>(inner),
                 reinterpret_cast<const skity::Vec2*>(inner_radii));
  c->DrawDRRect(o, i, *p);
}

void skity_canvas_get_global_clip_bounds(skity_canvas canvas, skity_rect* out) {
  auto* c = canvas_of(canvas);
  if (c == nullptr || out == nullptr) {
    return;
  }
  const skity::Rect& r = c->GetGlobalClipBounds();
  out->left = r.Left();
  out->top = r.Top();
  out->right = r.Right();
  out->bottom = r.Bottom();
}

}  // extern "C"
