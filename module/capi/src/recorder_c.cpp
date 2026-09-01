// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_recorder.h>

#include <algorithm>
#include <skity/recorder/display_list.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/recorder/recording_canvas.hpp>
#include <skity/render/canvas.hpp>
#include <utility>
#include <vector>

#include "handle.hpp"

namespace {

skity::PictureRecorder* recorder_of(skity_picture_recorder handle) {
  auto* w = skity::capi::resolve<skity_picture_recorder_s>(
      handle, SKITY_OBJECT_TYPE_PICTURE_RECORDER);
  return w ? static_cast<skity::PictureRecorder*>(w->impl.get()) : nullptr;
}

skity::DisplayList* display_list_of(skity_display_list handle) {
  auto* w = skity::capi::resolve<skity_display_list_s>(
      handle, SKITY_OBJECT_TYPE_DISPLAY_LIST);
  return w ? static_cast<skity::DisplayList*>(w->impl.get()) : nullptr;
}

skity::Canvas* canvas_of(skity_canvas handle) {
  auto* w =
      skity::capi::resolve<skity_canvas_s>(handle, SKITY_OBJECT_TYPE_CANVAS);
  return w ? static_cast<skity::Canvas*>(w->impl.get()) : nullptr;
}

}  // namespace

extern "C" {

skity_picture_recorder skity_picture_recorder_create(void) {
  return skity::capi::alloc_handle<skity_picture_recorder_s>(
      SKITY_OBJECT_TYPE_PICTURE_RECORDER, SKITY_HANDLE_OWNING,
      std::make_shared<skity::PictureRecorder>());
}

void skity_picture_recorder_destroy(skity_picture_recorder recorder) {
  skity::capi::destroy_handle<skity_picture_recorder_s>(
      recorder, SKITY_OBJECT_TYPE_PICTURE_RECORDER);
}

void skity_picture_recorder_begin(skity_picture_recorder recorder,
                                  const skity_rect* bounds) {
  auto* r = recorder_of(recorder);
  if (r == nullptr) return;
  if (bounds != nullptr) {
    r->BeginRecording(*reinterpret_cast<const skity::Rect*>(bounds));
  } else {
    r->BeginRecording();
  }
}

void skity_picture_recorder_begin_with_options(
    skity_picture_recorder recorder, const skity_rect* bounds,
    const skity_display_list_build_options* options) {
  auto* r = recorder_of(recorder);
  if (r == nullptr) return;
  skity::DisplayListBuildOptions opts;
  if (options != nullptr) {
    opts.build_rtree = options->build_rtree != 0;
  }
  if (bounds != nullptr) {
    r->BeginRecording(*reinterpret_cast<const skity::Rect*>(bounds), opts);
  } else {
    r->BeginRecording(skity::Rect::MakeLTRB(-1E9F, -1E9F, 1E9F, 1E9F), opts);
  }
}

int32_t skity_picture_recorder_get_last_op_offset(
    skity_picture_recorder recorder) {
  auto* r = recorder_of(recorder);
  if (r == nullptr) return -1;
  return r->GetRecordingCanvas()->GetLastOpOffset().GetValue();
}

skity_canvas skity_picture_recorder_get_canvas(
    skity_picture_recorder recorder) {
  auto* r = recorder_of(recorder);
  if (r == nullptr) return nullptr;
  // The RecordingCanvas is owned by the recorder; wrap it non-owning.
  skity::Canvas* raw = r->GetRecordingCanvas();
  if (raw == nullptr) return nullptr;
  std::shared_ptr<skity::Canvas> impl(raw, [](skity::Canvas*) {});
  return skity::capi::alloc_handle<skity_canvas_s>(SKITY_OBJECT_TYPE_CANVAS, 0,
                                                   std::move(impl));
}

skity_result skity_picture_recorder_finish(skity_picture_recorder recorder,
                                           skity_display_list* out) {
  auto* r = recorder_of(recorder);
  if (r == nullptr || out == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  std::unique_ptr<skity::DisplayList> dl = r->FinishRecording();
  if (dl == nullptr) {
    return SKITY_ERROR_INVALID_ARGUMENT;
  }
  std::shared_ptr<skity::DisplayList> impl(dl.release());
  skity_display_list_s* w = skity::capi::alloc_handle<skity_display_list_s>(
      SKITY_OBJECT_TYPE_DISPLAY_LIST, SKITY_HANDLE_OWNING, std::move(impl));
  if (w == nullptr) {
    return SKITY_ERROR_OUT_OF_HOST_MEMORY;
  }
  *out = w;
  return SKITY_SUCCESS;
}

void skity_display_list_destroy(skity_display_list list) {
  skity::capi::destroy_handle<skity_display_list_s>(
      list, SKITY_OBJECT_TYPE_DISPLAY_LIST);
}

void skity_display_list_draw(skity_display_list list, skity_canvas canvas) {
  auto* dl = display_list_of(list);
  auto* c = canvas_of(canvas);
  if (dl != nullptr && c != nullptr) {
    dl->Draw(c);
  }
}

void skity_display_list_draw_with_cull_rect(skity_display_list list,
                                            skity_canvas canvas,
                                            const skity_rect* cull_rect) {
  auto* dl = display_list_of(list);
  auto* c = canvas_of(canvas);
  if (dl == nullptr || c == nullptr) {
    return;
  }
  if (cull_rect == nullptr) {
    return;
  }
  dl->Draw(c, *reinterpret_cast<const skity::Rect*>(cull_rect));
}

uint32_t skity_display_list_search(skity_display_list list,
                                   const skity_rect* rect, int32_t* out_offsets,
                                   uint32_t capacity) {
  auto* dl = display_list_of(list);
  if (dl == nullptr || rect == nullptr) {
    return 0;
  }
  auto offsets = dl->Search(*reinterpret_cast<const skity::Rect*>(rect));
  uint32_t count = (uint32_t)offsets.size();
  if (out_offsets != nullptr && capacity > 0) {
    uint32_t fill = std::min(count, capacity);
    for (uint32_t i = 0; i < fill; i++) {
      out_offsets[i] = offsets[i].GetValue();
    }
  }
  return count;
}

uint32_t skity_display_list_search_non_overlapping_drawn_rects(
    skity_display_list list, const skity_rect* rect, skity_rect* out_rects,
    uint32_t capacity) {
  auto* dl = display_list_of(list);
  if (dl == nullptr || rect == nullptr) {
    return 0;
  }
  auto rects = dl->SearchNonOverlappingDrawnRects(
      *reinterpret_cast<const skity::Rect*>(rect));
  uint32_t count = (uint32_t)rects.size();
  if (out_rects != nullptr && capacity > 0) {
    uint32_t fill = std::min(count, capacity);
    for (uint32_t i = 0; i < fill; i++) {
      out_rects[i].left = rects[i].Left();
      out_rects[i].top = rects[i].Top();
      out_rects[i].right = rects[i].Right();
      out_rects[i].bottom = rects[i].Bottom();
    }
  }
  return count;
}

void skity_display_list_get_bounds(skity_display_list list,
                                   skity_rect* out_bounds) {
  auto* dl = display_list_of(list);
  if (dl == nullptr || out_bounds == nullptr) return;
  skity::Rect b = dl->GetBounds();
  out_bounds->left = b.Left();
  out_bounds->top = b.Top();
  out_bounds->right = b.Right();
  out_bounds->bottom = b.Bottom();
}

uint32_t skity_display_list_get_op_count(skity_display_list list) {
  auto* dl = display_list_of(list);
  return dl != nullptr ? dl->OpCount() : 0u;
}

uint32_t skity_display_list_get_properties(skity_display_list list) {
  uint32_t props = 0;
  auto* dl = display_list_of(list);
  if (dl == nullptr) return 0;
  if (dl->HasSaveLayer()) props |= SKITY_DISPLAY_LIST_PROPERTY_SAVE_LAYER;
  if (dl->HasShader()) props |= SKITY_DISPLAY_LIST_PROPERTY_SHADER;
  if (dl->HasColorFilter()) props |= SKITY_DISPLAY_LIST_PROPERTY_COLOR_FILTER;
  if (dl->HasMaskFilter()) props |= SKITY_DISPLAY_LIST_PROPERTY_MASK_FILTER;
  if (dl->HasImageFilter()) props |= SKITY_DISPLAY_LIST_PROPERTY_IMAGE_FILTER;
  return props;
}

skity_paint skity_display_list_get_op_paint_by_offset(skity_display_list list,
                                                      int32_t offset) {
  auto* dl = display_list_of(list);
  if (dl == nullptr) return nullptr;
  // RecordedOpOffset::Make is the public int round-trip factory; the value is
  // only meaningful for the display list it came from.
  skity::Paint* paint =
      dl->GetOpPaintByOffset(skity::RecordedOpOffset::Make(offset));
  if (paint == nullptr) return nullptr;
  // The paint lives inside the display list's storage; wrap it non-owning so
  // the handle stays valid for the list's lifetime and in-place edits are
  // visible to later replays.
  std::shared_ptr<skity::Paint> impl(paint, [](skity::Paint*) {});
  return skity::capi::alloc_handle<skity_paint_s>(SKITY_OBJECT_TYPE_PAINT, 0,
                                                  std::move(impl));
}

}  // extern "C"
