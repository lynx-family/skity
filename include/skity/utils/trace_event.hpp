// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_UTILS_TRACE_EVENT_HPP
#define INCLUDE_SKITY_UTILS_TRACE_EVENT_HPP

#include <cstdint>
#include <skity/macros.hpp>

namespace skity {

typedef void (*SkityTraceBeginSectionFunc)(
    const char* category_group, const char* section_name, int64_t trace_id,
    const char* arg1_name, const char* arg1_val, const char* arg2_name,
    const char* arg2_val);

typedef void (*SkityTraceEndSectionFunc)(const char* category_group,
                                         const char* section_name,
                                         int64_t trace_id);

typedef void (*SkityTraceCounterFunc)(const char* category, const char* name,
                                      uint64_t counter, bool incremental);

/**
 * Trace Handler for Skity.
 *
 * Outside user can inject their own trace handler to Skity. To extract trace
 * event from Skity engine.
 */
struct SkityTraceHandler {
  SkityTraceBeginSectionFunc begin_section = nullptr;
  SkityTraceEndSectionFunc end_section = nullptr;
  SkityTraceCounterFunc counter = nullptr;
};

/**
 * Inject custom trace handler to Skity.
 *
 * @note This is an experimental API which may be changed in the future. Also
 * this function must be called before other Skity APIs. otherwise it will cause
 * unpredicted behavior.
 *
 * @param handler custom trace handler.
 * @return true if inject successfully, otherwise false.
 */
SKITY_API bool InjectTraceHandler(const SkityTraceHandler& handler);

}  // namespace skity

#endif  // INCLUDE_SKITY_UTILS_TRACE_EVENT_HPP
