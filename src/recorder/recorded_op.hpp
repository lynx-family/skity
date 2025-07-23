// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RECORDER_RECORDED_OP_HPP
#define SRC_RECORDER_RECORDED_OP_HPP

#include <memory>
#include <skity/geometry/matrix.hpp>
#include <skity/geometry/rect.hpp>
#include <skity/graphic/paint.hpp>
#include <skity/render/canvas.hpp>
#include <skity/text/font.hpp>
#include <skity/text/glyph.hpp>
#include <skity/text/text_blob.hpp>

namespace skity {

#define FOR_EACH_RECORDED_OP(V) \
  V(Save)                       \
  V(Restore)                    \
  V(RestoreToCount)             \
  V(Translate)                  \
  V(Scale)                      \
  V(RotateByDegree)             \
  V(RotateByPoint)              \
  V(Skew)                       \
  V(Concat)                     \
  V(SetMatrix)                  \
  V(ResetMatrix)                \
  V(ClipRect)                   \
  V(ClipPath)                   \
  V(DrawLine)                   \
  V(DrawCircle)                 \
  V(DrawArc)                    \
  V(DrawOval)                   \
  V(DrawRect)                   \
  V(DrawRRect)                  \
  V(DrawRoundRect)              \
  V(DrawPath)                   \
  V(DrawPaint)                  \
  V(SaveLayer)                  \
  V(DrawTextBlob)               \
  V(DrawImage)                  \
  V(DrawGlyphs)

#define OP_TO_ENUM_VALUE(name) k##name,
enum class RecordedOpType { FOR_EACH_RECORDED_OP(OP_TO_ENUM_VALUE) };
#undef OP_TO_ENUM_VALUE

struct RecordedOp {
  RecordedOpType type : 8;
  uint32_t size : 24;
  explicit RecordedOp(RecordedOpType type) : type(type) {}
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Standard Skity Ops
////////////////////////////////////////////////////////////////////////////////////////////////////

struct SaveOp : RecordedOp {
  SaveOp() : RecordedOp(RecordedOpType::kSave) {}
};

struct RestoreOp : RecordedOp {
  RestoreOp() : RecordedOp(RecordedOpType::kRestore) {}
};

struct RestoreToCountOp : RecordedOp {
  explicit RestoreToCountOp(int& saveCount)
      : RecordedOp(RecordedOpType::kRestoreToCount), saveCount(saveCount) {}
  int saveCount;
};

struct TranslateOp : RecordedOp {
  TranslateOp(float& dx, float& dy)
      : RecordedOp(RecordedOpType::kTranslate), dx(dx), dy(dy) {}
  float dx;
  float dy;
};

struct ScaleOp : RecordedOp {
  ScaleOp(float& sx, float& sy)
      : RecordedOp(RecordedOpType::kScale), sx(sx), sy(sy) {}
  float sx;
  float sy;
};

struct RotateByDegreeOp : RecordedOp {
  explicit RotateByDegreeOp(float& degrees)
      : RecordedOp(RecordedOpType::kRotateByDegree), degrees(degrees) {}
  float degrees;
};

struct RotateByPointOp : RecordedOp {
  RotateByPointOp(float& degrees, float& px, float& py)
      : RecordedOp(RecordedOpType::kRotateByPoint),
        degrees(degrees),
        px(px),
        py(py) {}
  float degrees;
  float px;
  float py;
};

struct SkewOp : RecordedOp {
  SkewOp(float& sx, float& sy)
      : RecordedOp(RecordedOpType::kSkew), sx(sx), sy(sy) {}
  float sx;
  float sy;
};

struct ConcatOp : RecordedOp {
  explicit ConcatOp(const Matrix& matrix)
      : RecordedOp(RecordedOpType::kConcat), matrix(matrix) {}
  Matrix matrix;
};

struct SetMatrixOp : RecordedOp {
  explicit SetMatrixOp(const Matrix& matrix)
      : RecordedOp(RecordedOpType::kSetMatrix), matrix(matrix) {}
  Matrix matrix;
};

struct ResetMatrixOp : RecordedOp {
  ResetMatrixOp() : RecordedOp(RecordedOpType::kResetMatrix) {}
};

struct ClipRectOp : RecordedOp {
  explicit ClipRectOp(Rect const& rect,
                      Canvas::ClipOp op = Canvas::ClipOp::kIntersect)
      : RecordedOp(RecordedOpType::kClipRect), rect(rect), op(op) {}
  Rect rect;
  Canvas::ClipOp op;
};

struct ClipPathOp : RecordedOp {
  explicit ClipPathOp(Path const& path,
                      Canvas::ClipOp op = Canvas::ClipOp::kIntersect)
      : RecordedOp(RecordedOpType::kClipPath), path(path), op(op) {}
  Path path;
  Canvas::ClipOp op;
};

struct DrawLineOp : RecordedOp {
  DrawLineOp(float& x0, float& y0, float& x1, float& y1, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawLine),
        x0(x0),
        y0(y0),
        x1(x1),
        y1(y1),
        paint(paint) {}
  float x0;
  float y0;
  float x1;
  float y1;
  Paint paint;
};

struct DrawCircleOp : RecordedOp {
  DrawCircleOp(float& cx, float& cy, float& radius, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawCircle),
        cx(cx),
        cy(cy),
        radius(radius),
        paint(paint) {}
  float cx;
  float cy;
  float radius;
  Paint paint;
};

struct DrawArcOp : RecordedOp {
  DrawArcOp(Rect const& oval, float& startAngle, float& sweepAngle,
            bool useCenter, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawArc),
        oval(oval),
        startAngle(startAngle),
        sweepAngle(sweepAngle),
        useCenter(useCenter),
        paint(paint) {}
  Rect oval;
  float startAngle;
  float sweepAngle;
  bool useCenter;
  Paint paint;
};

struct DrawOvalOp : RecordedOp {
  DrawOvalOp(Rect const& oval, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawOval), oval(oval), paint(paint) {}
  Rect oval;
  Paint paint;
};

struct DrawRectOp : RecordedOp {
  DrawRectOp(Rect const& rect, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawRect), rect(rect), paint(paint) {}
  Rect rect;
  Paint paint;
};

struct DrawRRectOp : RecordedOp {
  DrawRRectOp(RRect const& rrect, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawRRect), rrect(rrect), paint(paint) {}
  RRect rrect;
  Paint paint;
};

struct DrawRoundRectOp : RecordedOp {
  DrawRoundRectOp(Rect const& rect, float& rx, float& ry, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawRoundRect),
        rect(rect),
        rx(rx),
        ry(ry),
        paint(paint) {}
  Rect rect;
  float rx;
  float ry;
  Paint paint;
};

struct DrawPathOp : RecordedOp {
  DrawPathOp(Path const& path, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawPath), path(path), paint(paint) {}
  Path path;
  Paint paint;
};

struct DrawPaintOp : RecordedOp {
  explicit DrawPaintOp(Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawPaint), paint(paint) {}
  Paint paint;
};

struct SaveLayerOp : RecordedOp {
  SaveLayerOp(Rect const& bounds, Paint const& paint)
      : RecordedOp(RecordedOpType::kSaveLayer), bounds(bounds), paint(paint) {}
  Rect bounds;
  Paint paint;
};

struct DrawTextBlobOp : RecordedOp {
  DrawTextBlobOp(const TextBlob* blob, float& x, float& y, Paint const& paint)
      : RecordedOp(RecordedOpType::kDrawTextBlob), x(x), y(y), paint(paint) {
    blob_ptr = std::make_unique<TextBlob>(blob->GetTextRun());
  }
  std::unique_ptr<TextBlob> blob_ptr;
  float x;
  float y;
  Paint paint;
};

struct DrawImageOp : RecordedOp {
  DrawImageOp(std::shared_ptr<Image> image, const Rect& src, const Rect& dst,
              const SamplingOptions& sampling, const Paint* paint_ = nullptr)
      : RecordedOp(RecordedOpType::kDrawImage),
        image(image),
        src(src),
        dst(dst),
        sampling(sampling) {
    if (paint_ != nullptr) {
      paint = *paint_;
    }
  }
  std::shared_ptr<Image> image;
  Rect src;
  Rect dst;
  SamplingOptions sampling;
  Paint paint;
};

struct DrawGlyphsOp : RecordedOp {
  DrawGlyphsOp(uint32_t& count, const GlyphID glyphs[],
               const float positions_x[], const float positions_y[],
               const Font& font, const Paint& paint)
      : RecordedOp(RecordedOpType::kDrawGlyphs),
        count(count),
        font(font),
        paint(paint) {
    m_glyphs.assign(glyphs, glyphs + count);
    m_positions_x.assign(positions_x, positions_x + count);
    m_positions_y.assign(positions_y, positions_y + count);
  }

  uint32_t count;
  std::vector<GlyphID> m_glyphs;
  std::vector<float> m_positions_x;
  std::vector<float> m_positions_y;
  const Font font;
  Paint paint;
};

}  // namespace skity

#endif  // SRC_RECORDER_RECORDED_OP_HPP
