// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity/recorder/display_list.hpp>

#include "src/recorder/recorded_op.hpp"

namespace skity {

DisplayList::DisplayList() {}

DisplayList::DisplayList(DisplayListStorage &&storage, size_t byte_count,
                         uint32_t op_count, const Rect &bounds)
    : storage_(std::move(storage)),
      byte_count_(byte_count),
      op_count_(op_count),
      bounds_(bounds) {}

DisplayList::~DisplayList() {
  uint8_t *ptr = storage_.get();
  DisposeOps(ptr, ptr + byte_count_);
}

Paint *DisplayList::GetOpPaintByOffset(RecordedOpOffset offset) {
  if (!offset.IsValid()) {
    return nullptr;
  }

  uint8_t *start = storage_.get();
  uint8_t *end = start + byte_count_;
  uint8_t *ptr = start + offset.GetValue();
  if (ptr < start || ptr >= end) {
    return nullptr;
  }

  auto op = reinterpret_cast<RecordedOp *>(ptr);
  switch (op->type) {
    case RecordedOpType::kDrawLine: {
      struct DrawLineOp *drawLineOp = static_cast<struct DrawLineOp *>(op);
      return &drawLineOp->paint;
    } break;
    case RecordedOpType::kDrawCircle: {
      struct DrawCircleOp *drawCircleOp =
          static_cast<struct DrawCircleOp *>(op);
      return &drawCircleOp->paint;
    } break;
    case RecordedOpType::kDrawArc: {
      struct DrawArcOp *drawArcOp = static_cast<struct DrawArcOp *>(op);
      return &drawArcOp->paint;
    } break;
    case RecordedOpType::kDrawOval: {
      struct DrawOvalOp *drawOvalOp = static_cast<struct DrawOvalOp *>(op);
      return &drawOvalOp->paint;
    } break;
    case RecordedOpType::kDrawRect: {
      struct DrawRectOp *drawRectOp = static_cast<struct DrawRectOp *>(op);
      return &drawRectOp->paint;
    } break;
    case RecordedOpType::kDrawRRect: {
      struct DrawRRectOp *drawRRectOp = static_cast<struct DrawRRectOp *>(op);
      return &drawRRectOp->paint;
    } break;
    case RecordedOpType::kDrawRoundRect: {
      struct DrawRoundRectOp *drawRoundRectOp =
          static_cast<struct DrawRoundRectOp *>(op);
      return &drawRoundRectOp->paint;
    } break;
    case RecordedOpType::kDrawPath: {
      struct DrawPathOp *drawPathOp = static_cast<struct DrawPathOp *>(op);
      return &drawPathOp->paint;
    } break;
    case RecordedOpType::kDrawPaint: {
      struct DrawPaintOp *drawPaintOp = static_cast<struct DrawPaintOp *>(op);
      return &drawPaintOp->paint;
    } break;
    case RecordedOpType::kSaveLayer: {
      struct SaveLayerOp *saveLayerOp = static_cast<struct SaveLayerOp *>(op);
      return &saveLayerOp->paint;
    } break;
    case RecordedOpType::kDrawTextBlob: {
      struct DrawTextBlobOp *drawTextBlobOp =
          static_cast<struct DrawTextBlobOp *>(op);
      return &drawTextBlobOp->paint;
    } break;
    case RecordedOpType::kDrawImage: {
      struct DrawImageOp *drawImageOp = static_cast<struct DrawImageOp *>(op);
      return &drawImageOp->paint;
    } break;
    case RecordedOpType::kDrawGlyphs: {
      struct DrawGlyphsOp *drawGlyphsOp =
          static_cast<struct DrawGlyphsOp *>(op);
      return &drawGlyphsOp->paint;
    } break;
    default:
      return nullptr;
  }
}

void DisplayList::DisposeOps(uint8_t *ptr, uint8_t *end) {
  while (ptr < end) {
    auto op = reinterpret_cast<const RecordedOp *>(ptr);
    ptr += op->size;

    switch (op->type) {
#define RECORDED_OP_DISPOSE(name)                      \
  case RecordedOpType::k##name:                        \
    if (!std::is_trivially_destructible_v<name##Op>) { \
      static_cast<const name##Op *>(op)->~name##Op();  \
    }                                                  \
    break;

      FOR_EACH_RECORDED_OP(RECORDED_OP_DISPOSE)
#undef RECORDED_OP_DISPOSE

      default:
        return;
    }
  }
}

void DisplayList::Draw(Canvas *canvas) {
  uint8_t *ptr = storage_.get();
  uint8_t *end = ptr + byte_count_;

  while (ptr < end) {
    auto op = reinterpret_cast<RecordedOp *>(ptr);
    ptr += op->size;
    switch (op->type) {
      case RecordedOpType::kSave: {
        canvas->Save();
      } break;
      case RecordedOpType::kRestore: {
        canvas->Restore();
      } break;
      case RecordedOpType::kRestoreToCount: {
        struct RestoreToCountOp *restoreToCountOp =
            static_cast<struct RestoreToCountOp *>(op);
        canvas->RestoreToCount(restoreToCountOp->saveCount);
      } break;
      case RecordedOpType::kTranslate: {
        struct TranslateOp *translateOp = static_cast<struct TranslateOp *>(op);
        canvas->Translate(translateOp->dx, translateOp->dy);
      } break;
      case RecordedOpType::kScale: {
        struct ScaleOp *scaleOp = static_cast<struct ScaleOp *>(op);
        canvas->Scale(scaleOp->sx, scaleOp->sy);
      } break;
      case RecordedOpType::kRotateByDegree: {
        struct RotateByDegreeOp *rotateByDegreeOp =
            static_cast<struct RotateByDegreeOp *>(op);
        canvas->Rotate(rotateByDegreeOp->degrees);
      } break;
      case RecordedOpType::kRotateByPoint: {
        struct RotateByPointOp *rotateByPointOp =
            static_cast<struct RotateByPointOp *>(op);
        canvas->Rotate(rotateByPointOp->degrees, rotateByPointOp->px,
                       rotateByPointOp->py);
      } break;
      case RecordedOpType::kSkew: {
        struct SkewOp *skewOp = static_cast<struct SkewOp *>(op);
        canvas->Skew(skewOp->sx, skewOp->sy);
      } break;
      case RecordedOpType::kConcat: {
        struct ConcatOp *concatOp = static_cast<struct ConcatOp *>(op);
        canvas->Concat(concatOp->matrix);
      } break;
      case RecordedOpType::kSetMatrix: {
        struct SetMatrixOp *setMatrixOp = static_cast<struct SetMatrixOp *>(op);
        canvas->SetMatrix(setMatrixOp->matrix);
      } break;
      case RecordedOpType::kResetMatrix: {
        canvas->ResetMatrix();
      } break;
      case RecordedOpType::kClipRect: {
        struct ClipRectOp *clipRectOp = static_cast<struct ClipRectOp *>(op);
        canvas->ClipRect(clipRectOp->rect, clipRectOp->op);
      } break;
      case RecordedOpType::kClipPath: {
        struct ClipPathOp *clipPathOp = static_cast<struct ClipPathOp *>(op);
        canvas->ClipPath(clipPathOp->path, clipPathOp->op);
      } break;
      case RecordedOpType::kDrawLine: {
        struct DrawLineOp *drawLineOp = static_cast<struct DrawLineOp *>(op);
        canvas->DrawLine(drawLineOp->x0, drawLineOp->y0, drawLineOp->x1,
                         drawLineOp->y1, drawLineOp->paint);
      } break;
      case RecordedOpType::kDrawCircle: {
        struct DrawCircleOp *drawCircleOp =
            static_cast<struct DrawCircleOp *>(op);
        canvas->DrawCircle(drawCircleOp->cx, drawCircleOp->cy,
                           drawCircleOp->radius, drawCircleOp->paint);
      } break;
      case RecordedOpType::kDrawArc: {
        struct DrawArcOp *drawArcOp = static_cast<struct DrawArcOp *>(op);
        canvas->DrawArc(drawArcOp->oval, drawArcOp->startAngle,
                        drawArcOp->sweepAngle, drawArcOp->useCenter,
                        drawArcOp->paint);
      } break;
      case RecordedOpType::kDrawOval: {
        struct DrawOvalOp *drawOvalOp = static_cast<struct DrawOvalOp *>(op);
        canvas->DrawOval(drawOvalOp->oval, drawOvalOp->paint);
      } break;
      case RecordedOpType::kDrawRect: {
        struct DrawRectOp *drawRectOp = static_cast<struct DrawRectOp *>(op);
        canvas->DrawRect(drawRectOp->rect, drawRectOp->paint);
      } break;
      case RecordedOpType::kDrawRRect: {
        struct DrawRRectOp *drawRRectOp = static_cast<struct DrawRRectOp *>(op);
        canvas->DrawRRect(drawRRectOp->rrect, drawRRectOp->paint);
      } break;
      case RecordedOpType::kDrawRoundRect: {
        struct DrawRoundRectOp *drawRoundRectOp =
            static_cast<struct DrawRoundRectOp *>(op);
        canvas->DrawRoundRect(drawRoundRectOp->rect, drawRoundRectOp->rx,
                              drawRoundRectOp->ry, drawRoundRectOp->paint);
      } break;
      case RecordedOpType::kDrawPath: {
        struct DrawPathOp *drawPathOp = static_cast<struct DrawPathOp *>(op);
        canvas->DrawPath(drawPathOp->path, drawPathOp->paint);
      } break;
      case RecordedOpType::kDrawPaint: {
        struct DrawPaintOp *drawPaintOp = static_cast<struct DrawPaintOp *>(op);
        canvas->DrawPaint(drawPaintOp->paint);
      } break;
      case RecordedOpType::kSaveLayer: {
        struct SaveLayerOp *saveLayerOp = static_cast<struct SaveLayerOp *>(op);
        canvas->SaveLayer(saveLayerOp->bounds, saveLayerOp->paint);
      } break;
      case RecordedOpType::kDrawTextBlob: {
        struct DrawTextBlobOp *drawTextBlobOp =
            static_cast<struct DrawTextBlobOp *>(op);
        canvas->DrawTextBlob(drawTextBlobOp->blob_ptr.get(), drawTextBlobOp->x,
                             drawTextBlobOp->y, drawTextBlobOp->paint);
      } break;
      case RecordedOpType::kDrawImage: {
        struct DrawImageOp *drawImageOp = static_cast<struct DrawImageOp *>(op);
        canvas->DrawImageRect(drawImageOp->image, drawImageOp->src,
                              drawImageOp->dst, drawImageOp->sampling,
                              &drawImageOp->paint);
      } break;
      case RecordedOpType::kDrawGlyphs: {
        struct DrawGlyphsOp *drawGlyphsOp =
            static_cast<struct DrawGlyphsOp *>(op);
        canvas->DrawGlyphs(drawGlyphsOp->count, &drawGlyphsOp->m_glyphs[0],
                           &drawGlyphsOp->m_positions_x[0],
                           &drawGlyphsOp->m_positions_y[0], drawGlyphsOp->font,
                           drawGlyphsOp->paint);
      } break;
    }
  }
}

}  // namespace skity
