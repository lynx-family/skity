// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_RENDER_AUTO_CANVAS_HPP
#define SRC_RENDER_AUTO_CANVAS_HPP

#include <skity/render/canvas.hpp>

namespace skity {

class AutoCanvasRestore {
 public:
  AutoCanvasRestore(Canvas* canvas, bool doSave)
      : canvas_(canvas), save_count_(0) {
    if (canvas_) {
      save_count_ = canvas->GetSaveCount();
      if (doSave) {
        canvas->Save();
      }
    }
  }

  /** Restores Canvas to saved state. Destructor is called when container goes
     out of scope.
  */
  ~AutoCanvasRestore() {
    if (canvas_) {
      canvas_->RestoreToCount(save_count_);
    }
  }

  /** Restores Canvas to saved state immediately. Subsequent calls and
      ~AutoCanvasRestore() have no effect.
  */
  void restore() {
    if (canvas_) {
      canvas_->RestoreToCount(save_count_);
      canvas_ = nullptr;
    }
  }

 private:
  Canvas* canvas_;
  int save_count_;

  AutoCanvasRestore(AutoCanvasRestore&&) = delete;
  AutoCanvasRestore(const AutoCanvasRestore&) = delete;
  AutoCanvasRestore& operator=(AutoCanvasRestore&&) = delete;
  AutoCanvasRestore& operator=(const AutoCanvasRestore&) = delete;
};

}  // namespace skity

#endif  // SRC_RENDER_AUTO_CANVAS_HPP
