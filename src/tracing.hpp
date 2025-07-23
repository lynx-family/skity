// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TRACING_HPP
#define SRC_TRACING_HPP

#include <skity/utils/trace_event.hpp>

namespace skity {

#ifdef SKITY_ENABLE_TRACING

class ScopedTraceEvent {
 public:
  ScopedTraceEvent(const char* name, int64_t trace_id);
  ~ScopedTraceEvent();

 private:
  const char* name_;
  int64_t trace_id_;
};

#define SKITY_TRACE_EVENT(name) ScopedTraceEvent name(#name, -1)

#else

#define SKITY_TRACE_EVENT(...)

#endif

}  // namespace skity

#endif  // SRC_TRACING_HPP
