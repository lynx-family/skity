// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <limits>
#include <skity/recorder/display_list.hpp>
#include <type_traits>
#include <vector>

#include "src/logging.hpp"
#include "src/recorder/display_list_rtree.hpp"
#include "src/recorder/recorded_op.hpp"

namespace skity {

namespace {

bool IsReplayStateOp(RecordedOpType type) {
  switch (type) {
    case RecordedOpType::kTranslate:
    case RecordedOpType::kScale:
    case RecordedOpType::kRotateByDegree:
    case RecordedOpType::kRotateByPoint:
    case RecordedOpType::kSkew:
    case RecordedOpType::kConcat:
    case RecordedOpType::kSetMatrix:
    case RecordedOpType::kResetMatrix:
    case RecordedOpType::kClipRect:
    case RecordedOpType::kClipRRect:
    case RecordedOpType::kClipPath:
      return true;
    default:
      return false;
  }
}

bool IsReplayDrawOp(RecordedOpType type) {
  switch (type) {
    case RecordedOpType::kDrawLine:
    case RecordedOpType::kDrawCircle:
    case RecordedOpType::kDrawArc:
    case RecordedOpType::kDrawOval:
    case RecordedOpType::kDrawRect:
    case RecordedOpType::kDrawRRect:
    case RecordedOpType::kDrawRoundRect:
    case RecordedOpType::kDrawDRRect:
    case RecordedOpType::kDrawPath:
    case RecordedOpType::kDrawPaint:
    case RecordedOpType::kDrawTextBlob:
    case RecordedOpType::kDrawImage:
    case RecordedOpType::kDrawGlyphs:
      return true;
    default:
      return false;
  }
}

int32_t GetReplayScopeEndOffset(const RecordedOp *op) {
  switch (op->type) {
    case RecordedOpType::kSave:
      return static_cast<const SaveOp *>(op)->restore_offset;
    case RecordedOpType::kSaveLayer:
      return static_cast<const SaveLayerOp *>(op)->restore_offset;
    default:
      return -1;
  }
}

void ReplayRecordedOp(Canvas *canvas, const RecordedOp *op) {
  switch (op->type) {
    case RecordedOpType::kSave: {
      canvas->Save();
    } break;
    case RecordedOpType::kRestore: {
      canvas->Restore();
    } break;
    case RecordedOpType::kTranslate: {
      auto *translate_op = static_cast<const TranslateOp *>(op);
      canvas->Translate(translate_op->dx, translate_op->dy);
    } break;
    case RecordedOpType::kScale: {
      auto *scale_op = static_cast<const ScaleOp *>(op);
      canvas->Scale(scale_op->sx, scale_op->sy);
    } break;
    case RecordedOpType::kRotateByDegree: {
      auto *rotate_by_degree_op = static_cast<const RotateByDegreeOp *>(op);
      canvas->Rotate(rotate_by_degree_op->degrees);
    } break;
    case RecordedOpType::kRotateByPoint: {
      auto *rotate_by_point_op = static_cast<const RotateByPointOp *>(op);
      canvas->Rotate(rotate_by_point_op->degrees, rotate_by_point_op->px,
                     rotate_by_point_op->py);
    } break;
    case RecordedOpType::kSkew: {
      auto *skew_op = static_cast<const SkewOp *>(op);
      canvas->Skew(skew_op->sx, skew_op->sy);
    } break;
    case RecordedOpType::kConcat: {
      auto *concat_op = static_cast<const ConcatOp *>(op);
      canvas->Concat(concat_op->matrix);
    } break;
    case RecordedOpType::kSetMatrix: {
      auto *set_matrix_op = static_cast<const SetMatrixOp *>(op);
      canvas->SetMatrix(set_matrix_op->matrix);
    } break;
    case RecordedOpType::kResetMatrix: {
      canvas->ResetMatrix();
    } break;
    case RecordedOpType::kClipRect: {
      auto *clip_rect_op = static_cast<const ClipRectOp *>(op);
      canvas->ClipRect(clip_rect_op->rect, clip_rect_op->op);
    } break;
    case RecordedOpType::kClipRRect: {
      auto *clip_rrect_op = static_cast<const ClipRRectOp *>(op);
      canvas->ClipRRect(clip_rrect_op->rrect, clip_rrect_op->op);
    } break;
    case RecordedOpType::kClipPath: {
      auto *clip_path_op = static_cast<const ClipPathOp *>(op);
      canvas->ClipPath(clip_path_op->path, clip_path_op->op);
    } break;
    case RecordedOpType::kDrawLine: {
      auto *draw_line_op = static_cast<const DrawLineOp *>(op);
      canvas->DrawLine(draw_line_op->x0, draw_line_op->y0, draw_line_op->x1,
                       draw_line_op->y1, draw_line_op->paint);
    } break;
    case RecordedOpType::kDrawCircle: {
      auto *draw_circle_op = static_cast<const DrawCircleOp *>(op);
      canvas->DrawCircle(draw_circle_op->cx, draw_circle_op->cy,
                         draw_circle_op->radius, draw_circle_op->paint);
    } break;
    case RecordedOpType::kDrawArc: {
      auto *draw_arc_op = static_cast<const DrawArcOp *>(op);
      canvas->DrawArc(draw_arc_op->oval, draw_arc_op->startAngle,
                      draw_arc_op->sweepAngle, draw_arc_op->useCenter,
                      draw_arc_op->paint);
    } break;
    case RecordedOpType::kDrawOval: {
      auto *draw_oval_op = static_cast<const DrawOvalOp *>(op);
      canvas->DrawOval(draw_oval_op->oval, draw_oval_op->paint);
    } break;
    case RecordedOpType::kDrawRect: {
      auto *draw_rect_op = static_cast<const DrawRectOp *>(op);
      canvas->DrawRect(draw_rect_op->rect, draw_rect_op->paint);
    } break;
    case RecordedOpType::kDrawRRect: {
      auto *draw_rrect_op = static_cast<const DrawRRectOp *>(op);
      canvas->DrawRRect(draw_rrect_op->rrect, draw_rrect_op->paint);
    } break;
    case RecordedOpType::kDrawRoundRect: {
      auto *draw_round_rect_op = static_cast<const DrawRoundRectOp *>(op);
      canvas->DrawRoundRect(draw_round_rect_op->rect, draw_round_rect_op->rx,
                            draw_round_rect_op->ry, draw_round_rect_op->paint);
    } break;
    case RecordedOpType::kDrawDRRect: {
      auto *draw_dr_rect_op = static_cast<const DrawDRRectOp *>(op);
      canvas->DrawDRRect(draw_dr_rect_op->outer, draw_dr_rect_op->inner,
                         draw_dr_rect_op->paint);
    } break;
    case RecordedOpType::kDrawPath: {
      auto *draw_path_op = static_cast<const DrawPathOp *>(op);
      canvas->DrawPath(draw_path_op->path, draw_path_op->paint);
    } break;
    case RecordedOpType::kDrawPaint: {
      auto *draw_paint_op = static_cast<const DrawPaintOp *>(op);
      canvas->DrawPaint(draw_paint_op->paint);
    } break;
    case RecordedOpType::kSaveLayer: {
      auto *save_layer_op = static_cast<const SaveLayerOp *>(op);
      canvas->SaveLayer(save_layer_op->bounds, save_layer_op->paint);
    } break;
    case RecordedOpType::kDrawTextBlob: {
      auto *draw_text_blob_op = static_cast<const DrawTextBlobOp *>(op);
      canvas->DrawTextBlob(draw_text_blob_op->blob_ptr.get(),
                           draw_text_blob_op->x, draw_text_blob_op->y,
                           draw_text_blob_op->paint);
    } break;
    case RecordedOpType::kDrawImage: {
      auto *draw_image_op = static_cast<const DrawImageOp *>(op);
      canvas->DrawImageRect(draw_image_op->image, draw_image_op->src,
                            draw_image_op->dst, draw_image_op->sampling,
                            &draw_image_op->paint);
    } break;
    case RecordedOpType::kDrawGlyphs: {
      auto *draw_glyphs_op = static_cast<const DrawGlyphsOp *>(op);
      DEBUG_CHECK(draw_glyphs_op->count <=
                  static_cast<uint32_t>(std::numeric_limits<int>::max()));
      canvas->DrawGlyphs(static_cast<int>(draw_glyphs_op->count),
                         draw_glyphs_op->m_glyphs.data(),
                         draw_glyphs_op->m_positions_x.data(),
                         draw_glyphs_op->m_positions_y.data(),
                         draw_glyphs_op->font, draw_glyphs_op->paint);
    } break;
  }
}

}  // namespace

DisplayList::DisplayList() {}

DisplayList::DisplayList(DisplayListStorage &&storage, size_t byte_count,
                         uint32_t op_count, const Rect &bounds,
                         uint32_t properties)
    : storage_(std::move(storage)),
      byte_count_(byte_count),
      op_count_(op_count),
      bounds_(bounds),
      properties_(properties) {}

DisplayList::~DisplayList() {
  uint8_t *ptr = const_cast<DisplayListStorage &>(storage_).get();
  DisposeOps(ptr, ptr + byte_count_);
}

Paint *DisplayList::GetOpPaintByOffset(RecordedOpOffset offset) {
  if (!offset.IsValid()) {
    return nullptr;
  }

  uint8_t *start = const_cast<DisplayListStorage &>(storage_).get();
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

void DisplayList::SetRTree(std::unique_ptr<DisplayListRTree> rtree) {
  rtree_ = std::move(rtree);
}

std::vector<RecordedOpOffset> DisplayList::Search(const Rect &rect) const {
  if (!rtree_) {
    return {};
  }
  return rtree_->Search(rect);
}

std::vector<Rect> DisplayList::SearchNonOverlappingDrawnRects(
    const Rect &rect) const {
  if (!rtree_) {
    return {};
  }
  return rtree_->SearchNonOverlappingDrawnRects(rect);
}

void DisplayList::DisposeOps(uint8_t *ptr, uint8_t *end) {
  while (ptr < end) {
    auto op = reinterpret_cast<const RecordedOp *>(ptr);
    ptr += op->size;

    switch (op->type) {
#define RECORDED_OP_DISPOSE(name)                           \
  case RecordedOpType::k##name:                             \
    if (!std::is_trivially_destructible<name##Op>::value) { \
      static_cast<const name##Op *>(op)->~name##Op();       \
    }                                                       \
    break;

      FOR_EACH_RECORDED_OP(RECORDED_OP_DISPOSE)
#undef RECORDED_OP_DISPOSE

      default:
        return;
    }
  }
}

void DisplayList::Draw(Canvas *canvas) const {
  if (canvas == nullptr) {
    return;
  }

  const uint8_t *ptr = storage_.get();
  const uint8_t *end = ptr + byte_count_;

  while (ptr < end) {
    auto op = reinterpret_cast<const RecordedOp *>(ptr);
    ptr += op->size;
    ReplayRecordedOp(canvas, op);
  }
}

void DisplayList::Draw(Canvas *canvas, const Rect &cull_rect) const {
  if (canvas == nullptr) {
    return;
  }

  if (cull_rect.IsEmpty()) {
    return;
  }

  if (!rtree_) {
    Draw(canvas);
    return;
  }

  auto target_offsets = Search(cull_rect);
  if (target_offsets.empty()) {
    return;
  }

  auto for_each_recorded_op = [this](auto &&fn) {
    const uint8_t *ptr = storage_.get();
    const uint8_t *end = ptr + byte_count_;
    int32_t offset = 0;
    while (ptr < end) {
      auto op = reinterpret_cast<const RecordedOp *>(ptr);
      fn(offset, op);
      ptr += op->size;
      offset += static_cast<int32_t>(op->size);
    }
  };

  struct SaveInfo {
    int32_t previous_scope_end = std::numeric_limits<int32_t>::max();
    int32_t restore_offset = std::numeric_limits<int32_t>::max();
    bool save_was_needed = false;
  };

  std::vector<SaveInfo> save_infos;
  auto next_target_it = target_offsets.begin();
  const auto target_end = target_offsets.end();
  int32_t next_target_offset = next_target_it != target_end
                                   ? next_target_it->GetValue()
                                   : std::numeric_limits<int32_t>::max();
  int32_t next_scope_end_offset = std::numeric_limits<int32_t>::max();
  for_each_recorded_op([&](int32_t offset, const RecordedOp *op) {
    while (next_target_it != target_end && offset > next_target_offset) {
      ++next_target_it;
      next_target_offset = next_target_it != target_end
                               ? next_target_it->GetValue()
                               : std::numeric_limits<int32_t>::max();
    }

    switch (op->type) {
      case RecordedOpType::kSave:
      case RecordedOpType::kSaveLayer: {
        const int32_t restore_offset = GetReplayScopeEndOffset(op);
        DEBUG_CHECK(restore_offset >= offset);
        const bool should_replay = next_target_offset < restore_offset;
        save_infos.push_back(
            {next_scope_end_offset, restore_offset, should_replay});
        next_scope_end_offset = restore_offset;
        if (should_replay) {
          ReplayRecordedOp(canvas, op);
        }
      } break;
      case RecordedOpType::kRestore: {
        bool should_restore = false;
        if (!save_infos.empty() && save_infos.back().restore_offset == offset) {
          should_restore = save_infos.back().save_was_needed;
          next_scope_end_offset = save_infos.back().previous_scope_end;
          save_infos.pop_back();
        }
        if (should_restore) {
          canvas->Restore();
        }
      } break;
      default:
        if (IsReplayStateOp(op->type)) {
          if (next_target_offset < next_scope_end_offset) {
            ReplayRecordedOp(canvas, op);
          }
          break;
        }
        if (!IsReplayDrawOp(op->type) || offset != next_target_offset) {
          break;
        }
        ReplayRecordedOp(canvas, op);
        ++next_target_it;
        next_target_offset = next_target_it != target_end
                                 ? next_target_it->GetValue()
                                 : std::numeric_limits<int32_t>::max();
        break;
    }
  });
}

}  // namespace skity
