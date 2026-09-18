// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_RECORDER_HPP
#define MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_RECORDER_HPP

// PictureRecorder / DisplayList wrappers of the header-only RAII layer:
// record canvas draw commands once, replay them any number of times (option
// for partial redraw through an RTree). The recording canvas is borrowed
// from the recorder (non-owning view, invalidated by FinishRecording). See
// skity.hpp for the layer's design.

#include <skity_c/skity_recorder.h>

#include <cstdint>
#include <skity_hpp/skity_base.hpp>
#include <skity_hpp/skity_canvas.hpp>
#include <skity_hpp/skity_paint.hpp>
#include <skity_hpp/skity_types.hpp>

#include <vector>

namespace skity {
namespace raii {

/** Build options, mirroring the legacy skity::DisplayListBuildOptions. */
struct DisplayListBuildOptions {
  /**
   * Build an RTree of op bounds; required for Search / non-overlapping-rect
   * queries and effective cull-rect drawing (both otherwise fall back to
   * whole-list behavior).
   */
  bool build_rtree = false;

  skity_display_list_build_options ToC() const {
    skity_display_list_build_options o{};
    o.build_rtree = build_rtree ? 1u : 0u;
    return o;
  }
};

/** A recorded, replayable set of draw commands (legacy skity::DisplayList). */
class DisplayList
    : public detail::OwnHandle<skity_display_list, skity_display_list_destroy> {
 public:
  /** Property bits of the recorded content (legacy Has*() getters). */
  enum class Property : uint32_t {
    kNone = SKITY_DISPLAY_LIST_PROPERTY_NONE,
    kSaveLayer = SKITY_DISPLAY_LIST_PROPERTY_SAVE_LAYER,
    kShader = SKITY_DISPLAY_LIST_PROPERTY_SHADER,
    kColorFilter = SKITY_DISPLAY_LIST_PROPERTY_COLOR_FILTER,
    kMaskFilter = SKITY_DISPLAY_LIST_PROPERTY_MASK_FILTER,
    kImageFilter = SKITY_DISPLAY_LIST_PROPERTY_IMAGE_FILTER,
  };

  /** Replay the whole list onto @p canvas. */
  void Draw(const Canvas& canvas) const {
    skity_display_list_draw(get(), canvas.get());
  }

  /**
   * Replay only the ops intersecting @p cull_rect (partial redraw), keeping
   * the save / clip / matrix state those commands depend on. Requires a
   * build_rtree recording; otherwise falls back to a whole-list replay. An
   * empty rect draws nothing.
   */
  void Draw(const Canvas& canvas, const Rect& cull_rect) const {
    skity_display_list_draw_with_cull_rect(get(), canvas.get(), &cull_rect);
  }

  /** Cull bounds of the recorded content. */
  Rect GetBounds() const {
    Rect out;
    skity_display_list_get_bounds(get(), &out);
    return out;
  }

  /** Number of recorded draw ops. */
  uint32_t GetOpCount() const {
    return skity_display_list_get_op_count(get());
  }

  /** Property bitmask (a combination of Property values). */
  uint32_t GetProperties() const {
    return skity_display_list_get_properties(get());
  }
  bool HasProperty(Property bit) const {
    return (GetProperties() & static_cast<uint32_t>(bit)) != 0;
  }

  /**
   * Offsets of the ops whose device-space bounds intersect @p rect, in
   * recording order; empty when nothing intersects. Requires build_rtree.
   */
  std::vector<int32_t> Search(const Rect& rect) const {
    uint32_t count = skity_display_list_search(get(), &rect, nullptr, 0);
    std::vector<int32_t> offsets(count);
    if (count > 0) {
      skity_display_list_search(get(), &rect, offsets.data(), count);
    }
    return offsets;
  }

  /**
   * Damage region covering the ops intersecting @p rect, merged into
   * mutually non-overlapping rectangles; useful to union with a surface
   * damage region before a cull-rect replay. Requires build_rtree.
   */
  std::vector<Rect> SearchNonOverlappingDrawnRects(const Rect& rect) const {
    uint32_t count = skity_display_list_search_non_overlapping_drawn_rects(
        get(), &rect, nullptr, 0);
    std::vector<Rect> rects(count);
    if (count > 0) {
      skity_display_list_search_non_overlapping_drawn_rects(
          get(), &rect, rects.data(), count);
    }
    return rects;
  }

  /**
   * Paint of the op recorded at @p offset (from Search or
   * PictureRecorder::GetLastOpOffset), for in-place modification before
   * replay. The paint lives inside the list's storage, so the returned
   * wrapper borrows it (valid until this list wrapper is destroyed; the
   * wrapper's destruction only reclaims the C handle).
   */
  Paint GetOpPaintByOffset(int32_t offset) const {
    return Paint::Adopt(
        skity_display_list_get_op_paint_by_offset(get(), offset));
  }

 private:
  explicit DisplayList(skity_display_list h) : OwnHandle(h) {}
  friend class PictureRecorder;
};

/** Records draw commands into a DisplayList (legacy shape). */
class PictureRecorder
    : public detail::OwnHandle<skity_picture_recorder,
                               skity_picture_recorder_destroy> {
 public:
  PictureRecorder() : OwnHandle(skity_picture_recorder_create()) {}

  /** Begin recording with default build options. */
  void BeginRecording(const Rect& bounds = Rect::MakeWH(0.f, 0.f)) {
    skity_picture_recorder_begin(get(), &bounds);
  }

  /** Begin recording with explicit build options. */
  void BeginRecording(const Rect& bounds,
                      const DisplayListBuildOptions& options) {
    skity_display_list_build_options c_options = options.ToC();
    skity_picture_recorder_begin_with_options(get(), &bounds, &c_options);
  }

  /**
   * The recording canvas (borrowed view, owned by the recorder); stays
   * valid until FinishRecording or destruction. NULL-view when recording
   * has not begun.
   */
  Canvas GetRecordingCanvas() const {
    return Canvas(skity_picture_recorder_get_canvas(get()));
  }

  /**
   * Storage offset of the most recently recorded op, for
   * DisplayList::GetOpPaintByOffset; -1 when nothing has been recorded.
   * Call right after the draw of interest, while still recording.
   */
  int32_t GetLastOpOffset() const {
    return skity_picture_recorder_get_last_op_offset(get());
  }

  /**
   * Finish recording and produce the display list; invalidates the recording
   * canvas view. Returns an empty wrapper when recording has not begun.
   */
  DisplayList FinishRecording() {
    skity_display_list out = nullptr;
    skity_result result = skity_picture_recorder_finish(get(), &out);
    return result == SKITY_SUCCESS ? DisplayList(out) : DisplayList(nullptr);
  }
};

}  // namespace raii
}  // namespace skity

#endif  // MODULE_CAPI_INCLUDE_SKITY_HPP_SKITY_RECORDER_HPP
