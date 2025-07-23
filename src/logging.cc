// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/logging.hpp"

#include <cstdlib>
#include <cstring>
#include <skity/io/logger.hpp>

#ifdef SKITY_LOG
#include <fmt/color.h>
#ifdef SKITY_ANDROID
#include <android/log.h>
#elif defined(SKITY_HARMONY)
#include <hilog/log.h>
#endif
#endif  // SKITY_LOG

#ifdef SKITY_HARMONY
const unsigned int LOG_PRINT_DOMAIN = 0xFF00;
#endif

namespace skity {

const char* StripPath(const char* path) {
  auto* p = strrchr(path, '/');
  if (p) {
    return p + 1;
  }
  return path;
}

#ifdef SKITY_LOG
Logger::LogHandler g_custom_log_i = nullptr;
Logger::LogHandler g_custom_log_d = nullptr;
Logger::LogHandler g_custom_log_e = nullptr;

void Log::WriteInfo(const std::string& msg) {
  if (g_custom_log_i) {
    g_custom_log_i(msg.c_str());
  } else {
#ifdef SKITY_ANDROID
    __android_log_print(ANDROID_LOG_INFO, "skity", "%s", msg.c_str());
#elif defined(SKITY_HARMONY)
    OH_LOG_Print(LOG_APP, LOG_INFO, LOG_PRINT_DOMAIN, "skity", "%{public}s",
                 msg.c_str());
#else
    auto style = fmt::fg(fmt::color::green);
    fmt::print(style, "{}\n", msg);
#endif
  }
}

void Log::WriteWarn(const std::string& msg) {
  if (g_custom_log_i) {
    g_custom_log_i(msg.c_str());
  } else {
#ifdef SKITY_ANDROID
    __android_log_print(ANDROID_LOG_WARN, "skity", "%s", msg.c_str());
#elif defined(SKITY_HARMONY)
    OH_LOG_Print(LOG_APP, LOG_WARN, LOG_PRINT_DOMAIN, "skity", "%{public}s",
                 msg.c_str());
#else
    auto style = fmt::fg(fmt::color::yellow);
    fmt::print(style, "{}\n", msg);
#endif
  }
}

void Log::WriteError(const std::string& msg) {
  if (g_custom_log_i) {
    g_custom_log_i(msg.c_str());
  } else {
#ifdef SKITY_ANDROID
    __android_log_print(ANDROID_LOG_ERROR, "skity", "%s", msg.c_str());
#elif defined(SKITY_HARMONY)
    OH_LOG_Print(LOG_APP, LOG_ERROR, LOG_PRINT_DOMAIN, "skity", "%{public}s",
                 msg.c_str());
#else
    auto style = fmt::fg(fmt::color::red);
    fmt::print(style, "{}\n", msg);
#endif
  }
}

void Log::WriteDebug(const std::string& msg) {
  if (g_custom_log_i) {
    g_custom_log_i(msg.c_str());
  } else {
#ifdef SKITY_ANDROID
    __android_log_print(ANDROID_LOG_DEBUG, "skity", "%s", msg.c_str());
#elif defined(SKITY_HARMONY)
    OH_LOG_Print(LOG_APP, LOG_DEBUG, LOG_PRINT_DOMAIN, "skity", "%{public}s",
                 msg.c_str());
#else
    fmt::print("{}\n", msg);
#endif
  }
}

void Log::Init() {}

#endif  // SKITY_LOG

void KillProcess() { abort(); }

void Logger::RegisteLog(CustomLogger* log) {
#ifdef SKITY_LOG
  if (log) {
    g_custom_log_i = log->log_i;
    g_custom_log_d = log->log_d;
    g_custom_log_e = log->log_e;
  } else {
    g_custom_log_i = nullptr;
    g_custom_log_d = nullptr;
    g_custom_log_e = nullptr;
  }
#endif
}

}  // namespace skity
