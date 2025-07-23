// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_RENDER_CANVAS_HPP
#define INCLUDE_SKITY_RENDER_CANVAS_HPP

#include <memory>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/image.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/graphic/path.hpp>
#include <skity/graphic/sampling_options.hpp>
#include <skity/macros.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/typeface.hpp>

namespace skity {

class Bitmap;
class CanvasState;
class Pixmap;
class TextBlob;
class Font;

constexpr Rect kMaxCullRect = Rect::MakeLTRB(-1E9F, -1E9F, 1E9F, 1E9F);

/**
 * @class Canvas
 * Provide an interface for drawing.
 */
class SKITY_API Canvas {
 public:
  explicit Canvas(Rect cull_rect = kMaxCullRect);
  Canvas(const Canvas&) = delete;
  Canvas& operator=(const Canvas&) = delete;
  virtual ~Canvas();

  /**
   * Save Matrix and clip.
   *
   * @return  depth of saved stack
   */
  int Save();

  /**
   * Removes changes to Matrix and clip sice canvas was last saved.
   */
  void Restore();

  /**
   * @return depth of save state stack
   */
  int GetSaveCount() const;

  /**
   * Restores state to Matrix and clip values when save() returned saveCount.
   *
   * Does nothing if saveCount is greater than state stack count.
   *
   */
  void RestoreToCount(int saveCount);

  /**
   * Translates Matrix by dx along the x-axis and dy along the y-axis.
   *
   * @param dx distance to translate on x-axis
   * @param dy distance to translate on y-axis
   */
  void Translate(float dx, float dy);

  /**
   * Scale Matrix by sx on x-asix and sy on y-axis
   *
   * @param sx amount to scale on x-axis
   * @param sy amount to scale on y-axis
   */
  void Scale(float sx, float sy);

  /**
   * Rotate Matrix by degrees. Positive degrees rotates clockwise.
   *
   * @param degrees amount to rotate, in degrees
   */
  void Rotate(float degrees);

  /**
   * Rotates Matrix by degrees about a point at(px, py).
   *
   * @param degrees amount to rotate, in degrees
   * @param px      x-axis value of the point to rotate about
   * @param py      y-axis value of the point to rotate about
   */
  void Rotate(float degrees, float px, float py);

  /**
   * Skews Matrix by sx on x-axis and sy on the y-axis.
   *
   * @param sx  amount to skew on x-axis
   * @param sy  amount to skew on y-axis
   */
  void Skew(float sx, float sy);

  /**
   * Replaces Matrix with matrix premultiplied with existing Matrix.
   * @param matrix matrix to premultiply with existing Matrix
   */
  void Concat(const Matrix& matrix);

  /**
   * Replaces Matrix with the given matrix.
   * @param matrix matrix to replace existing Matrix
   */
  void SetMatrix(const Matrix& matrix);

  /**
   * Reset current Matrix to identity matrix.
   */
  void ResetMatrix();

  /**
   * Returns the current transform matrix applied to canvas
   * @return existing Matrix applied on this canvas
   */
  Matrix GetTotalMatrix() const;

  enum class ClipOp {
    kDifference,
    kIntersect,
  };

  /**
   * Replaces clip with the intersection or difference of clip and rect
   *
   * @param rect  Rect to combine with clip
   * @param op    ClipOp to apply to clip
   */
  void ClipRect(Rect const& rect, ClipOp op = ClipOp::kIntersect);

  void ClipPath(Path const& path, ClipOp op = ClipOp::kIntersect);

  /**
   * Draws line segment from (x0, y0) to (x1, y1) using clip, Matrix, and paint.
   *
   * In paint: stroke width describes the line thicknes;
   * Paint::Cap draws the end rounded or square;
   * Paint::Style is ignored, as if were set to Paint::Style::kStroke_Style.
   *
   * @param x0    start of line segment on x-axis
   * @param y0    start of line segment on y-axis
   * @param x1    end of line segment on x-axis
   * @param y1    end of line segment on y-axis
   * @param paint stroke, blend, color, and so on, used to draw
   */
  void DrawLine(float x0, float y0, float x1, float y1, Paint const& paint);

  /**
   * Draws circle at center with radius using clip, Matrix, and Paint paint.
   *
   * @param cx      circle center on the x-axis
   * @param cy      circle center on the y-axis
   * @param radius  half the diameter of circle
   * @param paint   Paint stroke or fill, blend, color, and so on, used to draw
   */
  void DrawCircle(float cx, float cy, float radius, Paint const& paint);

  /**
   * Draws arc using clip, Matrix, and Paint.
   * Arc is part of oval bounded by oval, sweeping from startAngle to startAngle
   * plus sweepAngle. startAngle and sweepAngle are in degrees.
   *
   * startAngle of zero places start point at the right middle edge of oval.
   * A positive sweepAngle places arc end point clockwise from start point,
   * a negative sweepAngle places arc end point counterclockwise from start
   * point.
   * sweepAngle may exceed 360 degrees, a full circle.
   * If useCenter is true, draw a wedge that includes lines from oval center to
   * arc end points. If useCenter is false, draw arc between end points.
   *
   * If Rect oval is empty or sweepAngle is zero, nothing is drawn.
   *
   * @param oval        Rect bounds of oval containing arc to draw
   * @param startAngle  Angle in degrees where arc begins
   * @param sweepAngle  Sweep angle in degrees; positive is clockwise
   * @param useCenter   If true, include the center of the oval
   * @param paint       Paint stroke or fill, blend, color, and so on, used to
   *                    draw
   */
  void DrawArc(Rect const& oval, float startAngle, float sweepAngle,
               bool useCenter, Paint const& paint);

  /**
   * Draws oval oval using clip, Matrix, and Paint.
   *
   * @param oval    Rect bounds of oval
   * @param paint   Paint stroke or fill, blend, color, and so on, used to draw
   */
  void DrawOval(Rect const& oval, Paint const& paint);

  /**
   * Draws Rect rect using clip, Matrix, and Paint paint.
   *
   * @param rect  rectangle to draw
   * @param paint stroke or fill, blend, color, and so on, used to draw
   */
  void DrawRect(Rect const& rect, Paint const& paint);

  /**
   * Draws RRect rrect using clip, Matrix, and Paint paint.
   *
   * @param rrect RRect with up to eight corner radii to draw
   * @param paint Paint stroke or fill, blend, color, and so on, used to draw
   */
  void DrawRRect(RRect const& rrect, Paint const& paint);

  /**
   * Draws RRect bounded by Rect rect, with corner radii (rx, ry) using clip,
   * Matrix, and Paint paint.
   *
   * @param rect    Rect bounds of RRect to draw
   * @param rx      axis length of x-axis of oval describing rounded corners
   * @param ry      axis length on y-axis of oval describing rounded corners
   * @param paint   stroke, blend, color, and so on, used to draw
   */
  void DrawRoundRect(Rect const& rect, float rx, float ry, Paint const& paint);

  void DrawPath(Path const& path, Paint const& paint);

  /**
   * @brief       Fills clip with color color.
   *
   * @param color unpremultiplied color
   * @param mode  BlendMode used to combine source color and destination
   */
  void DrawColor(Color color, BlendMode mode = BlendMode::kSrcOver);
  void DrawColor(Color4f color, BlendMode mode = BlendMode::kSrcOver);

  /**
   * @brief       Fills clip with color color using BlendMode::kSrc.
   *
   * @param color unpremultiplied color
   */
  void Clear(Color color) { this->DrawColor(color, BlendMode::kSrc); }
  void Clear(Color4f color) { this->DrawColor(color, BlendMode::kSrc); }

  /**
   * @brief       Fills clip with Paint paint.
   *              Paint components, Color, Shader, ColorFilter, and BlendMode
   *              affect drawing.
   *
   * @param paint graphics state used to fill Canvas
   */
  void DrawPaint(Paint const& paint);

  /**
   * @brief         Save current matrix and clip, and allocate a backend render
   *                target for subsequent drawing.
   *                Calling Restore() discards changes to Matrix and clip, and
   *                draws the render target to current context.
   *
   * @note          For current implementation, calling RestoreToCount() may
   *                cause saved layer not drawing back to current context.
   *
   * @param bounds  location and size of backend render target.
   * @param paint   apply alpha, BlendMode and MaskFilter when drawing render
   *                target to current context.
   * @return int    depth of saved stack
   */
  int SaveLayer(Rect const& bounds, Paint const& paint);

  /**
   * @brief Flush the internal draw commands.
   * @note this function must be called if Canvas is create with GPU backend.
   *         After this funcion called on OpenGL backends:
   *                the stencil buffer maybe dirty and need clean
   *                the stencil mask、stencil func and stencil op is changed
   *                the color mask need reset.
   *         After this function called on Vulkan backends:
   *                the current VkCommandBuffer is filled with draw commands
   *                the binded VkPipeline is changed
   *
   */
  void Flush();

  /**
   * @deprecated  use drawSimpleText2 if need.
   */
  void DrawSimpleText(const char* text, float x, float y, Paint const& paint);

  /**
   * @deprecated  use drawTextBlob instead
   * @brief       this function is fallback to use drawTextBlob internal.
   *
   */
  void DrawSimpleText2(const char* text, float x, float y, Paint const& paint);

  Vec2 SimpleTextBounds(const char* text, Paint const& paint);

  void DrawTextBlob(const TextBlob* blob, float x, float y, Paint const& paint);

  void DrawTextBlob(std::shared_ptr<TextBlob> const& blob, float x, float y,
                    Paint const& paint) {
    DrawTextBlob(blob.get(), x, y, paint);
  }

  void DrawImage(const std::shared_ptr<Image>& image, float x, float y);

  void DrawImage(const std::shared_ptr<Image>& image, float x, float y,
                 const SamplingOptions& sampling, const Paint* = nullptr);

  void DrawImage(const std::shared_ptr<Image>&, const Rect& rect,
                 const Paint* = nullptr);

  void DrawImage(const std::shared_ptr<Image>&, const Rect& rect,
                 const SamplingOptions& sampling, const Paint* = nullptr);

  void DrawImageRect(const std::shared_ptr<Image>&, const Rect& src,
                     const Rect& dst, const SamplingOptions& sampling,
                     const Paint* = nullptr);

  void DrawGlyphs(int count, const GlyphID glyphs[], const float positions_x[],
                  const float positions_y[], const Font& font,
                  const Paint& paint);
  SKITY_EXPERIMENTAL
  inline void DrawDebugLine(bool debug) { draw_debug_line_ = debug; }

  SKITY_EXPERIMENTAL
  void UpdateViewport(uint32_t width, uint32_t height);
  uint32_t Width() const;
  uint32_t Height() const;

  virtual const Rect& GetGlobalClipBounds() const {
    return global_clip_bounds_stack_.back();
  }

  Rect GetLocalClipBounds() const;

  static std::unique_ptr<Canvas> MakeSoftwareCanvas(Bitmap* bitmap);

  bool QuickReject(const Rect& rect) const;

 protected:
  // default implement dispatch this to OnClipPath
  virtual void OnClipRect(Rect const& rect, ClipOp op);
  virtual void OnClipPath(Path const& path, ClipOp op) = 0;

  // default implement dispatch this to OnDrawPath
  virtual void OnDrawLine(float x0, float y0, float x1, float y1,
                          Paint const& paint);
  // default implement dispatch this to OnDrawPath
  virtual void OnDrawCircle(float cx, float cy, float radius,
                            Paint const& paint);
  // default implement dispatch this to OnDrawPath
  virtual void OnDrawOval(Rect const& oval, Paint const& paint);
  // default implement dispatch this to OnDrawPath
  virtual void OnDrawRect(Rect const& rect, Paint const& paint);
  // default implement dispatch this to OnDrawPath
  virtual void OnDrawRRect(RRect const& rrect, Paint const& paint);
  // default implement dispatch this to OnDrawPath
  virtual void OnDrawRoundRect(Rect const& rect, float rx, float ry,
                               Paint const& paint);

  virtual void OnDrawPath(Path const& path, Paint const& paint) = 0;

  virtual void OnSaveLayer(const Rect& bounds, const Paint& paint) = 0;

  virtual void OnDrawBlob(const TextBlob* blob, float x, float y,
                          Paint const& paint) = 0;

  virtual void OnDrawImageRect(std::shared_ptr<Image> image, const Rect& src,
                               const Rect& dst, const SamplingOptions& sampling,
                               Paint const* paint) = 0;

  virtual void OnDrawGlyphs(uint32_t count, const GlyphID glyphs[],
                            const float position_x[], const float position_y[],
                            const Font& font, const Paint& paint) = 0;

  virtual void OnDrawPaint(Paint const& paint) = 0;

  virtual void OnSave() = 0;
  virtual void OnRestore() = 0;
  virtual void OnRestoreToCount(int saveCount) = 0;
  virtual void OnTranslate(float dx, float dy) {}
  virtual void OnScale(float sx, float sy) {}
  virtual void OnRotate(float degree) {}
  virtual void OnRotate(float degree, float px, float py) {}
  virtual void OnSkew(float sx, float sy) {}
  virtual void OnConcat(Matrix const& matrix) {}
  virtual void OnSetMatrix(Matrix const& matrix) {}
  virtual void OnResetMatrix() {}
  virtual void OnFlush() = 0;
  virtual uint32_t OnGetWidth() const = 0;
  virtual uint32_t OnGetHeight() const = 0;

  virtual bool NeedGlyphPath(Paint const& paint);

  virtual void OnUpdateViewport(uint32_t width, uint32_t height) = 0;
  inline bool IsDrawDebugLine() const { return draw_debug_line_; }

  virtual CanvasState* GetCanvasState() const { return canvas_state_.get(); }

  void CalculateGlobalClipBounds(const Rect& local_clip_bounds, ClipOp op);

  void SetTracingCanvasState(bool tracing_canvas_state) {
    tracing_canvas_state_ = tracing_canvas_state;
  }

 private:
  void InternalSave();
  void InternalRestore();

 private:
  int32_t save_count_ = 1;
  bool draw_debug_line_ = false;
  std::vector<Rect> global_clip_bounds_stack_;
  bool tracing_canvas_state_ = true;
  std::unique_ptr<CanvasState> canvas_state_;
};

}  // namespace skity

#endif  // INCLUDE_SKITY_RENDER_CANVAS_HPP
