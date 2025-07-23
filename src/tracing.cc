// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/tracing.hpp"

namespace skity {

SkityTraceHandler g_trace_handler;

bool InjectTraceHandler(const SkityTraceHandler& handler) {
#ifdef SKITY_ENABLE_TRACING

  static bool inited = false;

  if (inited) {
    return false;
  }

  if (handler.begin_section == nullptr || handler.end_section == nullptr ||
      handler.counter == nullptr) {
    return false;
  }

  g_trace_handler = handler;
  inited = true;

  return true;

#else

  return false;

#endif
}

#ifdef SKITY_ENABLE_TRACING

constexpr const char* SKITY_TRACE_CATEGORY = "skity2d";

ScopedTraceEvent::ScopedTraceEvent(const char* name, int64_t trace_id)
    : name_(name), trace_id_(trace_id) {
  if (g_trace_handler.begin_section) {
    g_trace_handler.begin_section(SKITY_TRACE_CATEGORY, name_, trace_id_,
                                  nullptr, nullptr, nullptr, nullptr);
  }
}

ScopedTraceEvent::~ScopedTraceEvent() {
  if (g_trace_handler.end_section) {
    g_trace_handler.end_section(SKITY_TRACE_CATEGORY, name_, trace_id_);
  }
}

#endif

}  // namespace skity
