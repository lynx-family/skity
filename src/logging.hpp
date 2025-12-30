// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_LOGGING_HPP
#define SRC_LOGGING_HPP

#include <skity/macros.hpp>

#ifdef SKITY_LOG

// force disable fmt exceptions
#ifdef FMT_EXCEPTIONS
#undef FMT_EXCEPTIONS
#endif
#define FMT_EXCEPTIONS 0

#include <fmt/format.h>
#endif

namespace skity {

const char* StripPath(const char* path);

#ifdef SKITY_LOG
class Log {
 public:
  static void Init();

  template <typename... Args>
  static void Info(const char* fmt, Args... args) {
    std::string ms = "[skity] [INFO]";
    ms += fmt::format(fmt, args...);
    WriteInfo(ms);
  }

  template <typename... Args>
  static void Warn(const char* fmt, Args... args) {
    std::string ms = "[skity] [WARN]";
    ms += fmt::format(fmt, args...);
    WriteWarn(ms);
  }

  template <typename... Args>
  static void Error(const char* fmt, Args... args) {
    std::string ms = "[skity] [ERROR]";
    ms += fmt::format(fmt, args...);
    WriteError(ms);
  }

  template <typename... Args>
  static void Debug(const char* fmt, Args... args) {
    std::string ms = "[skity] [DEBUG]";
    ms += fmt::format(fmt, args...);
    WriteDebug(ms);
  }

 private:
  static void WriteInfo(const std::string& msg);
  static void WriteWarn(const std::string& msg);
  static void WriteError(const std::string& msg);
  static void WriteDebug(const std::string& msg);
};
#endif

[[noreturn]] void KillProcess();

}  // namespace skity

#ifdef SKITY_LOG

#ifndef SKITY_RELEASE
#define LOGI(...) skity::Log::Info(__VA_ARGS__)
#define LOGD(...) skity::Log::Debug(__VA_ARGS__)
#else
#define LOGI(...)
#define LOGD(...)
#endif

#define LOGW(...) skity::Log::Warn(__VA_ARGS__)
#define LOGE(...) skity::Log::Error(__VA_ARGS__)
#else  // SKITY_LOG
#define LOGI(...)
#define LOGW(...)
#define LOGE(...)
#define LOGD(...)
#endif  // SKITY_LOG

#define CHECK(condition)                                      \
  do {                                                        \
    if (!(condition)) {                                       \
      LOGE("Check Failed at file : {} line: {} {}",           \
           skity::StripPath(__FILE__), __LINE__, #condition); \
      skity::KillProcess();                                   \
    }                                                         \
  } while (false)

#ifdef NDEBUG
#define DEBUG_CHECK(condition)
#else
#define DEBUG_CHECK(condition) CHECK(condition)
#endif

#endif  // SRC_LOGGING_HPP
