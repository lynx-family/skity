// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <skity_c/skity_recorder.h>

#include <skity/recorder/display_list.hpp>
#include <skity/recorder/picture_recorder.hpp>
#include <skity/render/canvas.hpp>
#include <utility>

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

skity_canvas skity_picture_recorder_get_canvas(
    skity_picture_recorder recorder) {
  auto* r = recorder_of(recorder);
  if (r == nullptr) return nullptr;
  // The RecordingCanvas is owned by the recorder; wrap it non-owning.
  skity::Canvas* raw = r->GetRecordingCanvas();
  if (raw == nullptr) return nullptr;
  std::shared_ptr<void> impl(raw, [](void*) {});
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
  std::shared_ptr<void> impl(dl.release());
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

}  // extern "C"
