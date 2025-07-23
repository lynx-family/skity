// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "perf.hpp"

#include <cstdio>
#include <skity/graphic/paint.hpp>

#ifndef GL_ARB_timer_query
#define GL_TIME_ELAPSED 0x88BF
#endif

Perf::Perf(GraphRenderStyle style, std::string name)
    : name_(std::move(name)), style_(style), head_(0) {}

Perf::~Perf() {}

void Perf::StartGPUTimer() {}

void Perf::StopGPUTimer(float *, int) {}

void Perf::UpdateGraph(float frameTime) {
  head_ = (head_ + 1) % values_.size();
  values_[head_] = frameTime;
}

float Perf::GetGraphAverage() {
  float avg = 0.f;
  for (auto v : values_) {
    avg += v;
  }
  return avg / (float)values_.size();
}

void Perf::RenderGraph(skity::Canvas *canvas, float x, float y) {
  float avg, w, h;
  avg = GetGraphAverage();

  w = 200;
  h = 35;
  skity::Paint paint;
  paint.SetAntiAlias(true);
  paint.SetStyle(skity::Paint::kFill_Style);
  paint.SetColor(skity::ColorSetARGB(128, 0, 0, 0));
  skity::Rect rect = skity::Rect::MakeXYWH(x, y, w, h);
  canvas->DrawRect(rect, paint);

  skity::Path path;
  path.MoveTo(x, y + h);
  if (style_ == GRAPH_RENDER_FPS) {
    for (size_t i = 0; i < values_.size(); i++) {
      float v = 1.f / (0.00001f + values_[(head_ + i) % GRAPH_HISTORY_COUNT]);
      float vx, vy;
      if (v > 80.f) {
        v = 80.f;
      }
      vx = x + ((float)i / (GRAPH_HISTORY_COUNT - 1)) * w;
      vy = y + h - ((v / 80.f) * h);
      path.LineTo(vx, vy);
    }
  } else if (style_ == GRAPH_RENDER_PERCENT) {
    for (size_t i = 0; i < values_.size(); i++) {
      float v = values_[(head_ + i) % GRAPH_HISTORY_COUNT] * 1.f;
      float vx, vy;
      if (v > 100.f) {
        v = 100.f;
      }
      vx = x + ((float)i / (GRAPH_HISTORY_COUNT - 1)) * w;
      vy = y + h - ((v / 100.0f) * h);
      path.LineTo(vx, vy);
    }
  } else {
    for (size_t i = 0; i < values_.size(); i++) {
      float v = values_[(head_ + i) % GRAPH_HISTORY_COUNT] * 1000.f;
      float vx, vy;
      if (v > 20.f) {
        v = 20.f;
      }
      vx = x + ((float)i / (GRAPH_HISTORY_COUNT - 1)) * w;
      vy = y + h - ((v / 20.0f) * h);
      path.LineTo(vx, vy);
    }
  }

  path.LineTo(x + w, y + h);
  path.Close();
  paint.SetColor(skity::ColorSetARGB(128, 255, 192, 0));
  canvas->DrawPath(path, paint);

  if (!name_.empty()) {
    paint.SetTextSize(12.f);
    paint.SetColor(skity::ColorSetARGB(192, 240, 240, 240));
    canvas->DrawSimpleText2(name_.c_str(), x + 3, y + 3 + 14.f, paint);
  }

  char str[64];

  if (style_ == GRAPH_RENDER_FPS) {
    paint.SetTextSize(15.f);
    paint.SetColor(skity::ColorSetARGB(255, 240, 240, 240));
    std::sprintf(str, "%.2f FPS", 1.0f / avg);
    canvas->DrawSimpleText2(str, x + 100.f, y + 3.f + 15.f, paint);

    paint.SetTextSize(13.f);
    paint.SetColor(skity::ColorSetARGB(160, 240, 240, 240));
    sprintf(str, "%.2f ms", avg * 1000.f);
    canvas->DrawSimpleText2(str, x + 100.f, y + 3.f + 15.f + 14.f, paint);
  } else if (style_ == GRAPH_RENDER_PERCENT) {
    paint.SetTextSize(15.f);
    paint.SetColor(skity::ColorSetARGB(255, 240, 240, 240));
    sprintf(str, "%.1f %%", avg * 1.f);
    canvas->DrawSimpleText2(str, x + 60.f, y + 3.f + 15.f, paint);
  } else {
    paint.SetTextSize(15.f);
    paint.SetColor(skity::ColorSetARGB(255, 240, 240, 240));
    sprintf(str, "%.2f ms", avg * 1000.f);
    canvas->DrawSimpleText2(str, x + 100.f, y + 3.f + 15.f, paint);
  }
}
