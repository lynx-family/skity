// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef INCLUDE_SKITY_IO_LOGGER_HPP
#define INCLUDE_SKITY_IO_LOGGER_HPP

#include <skity/macros.hpp>

namespace skity {

/**
 * This is an experimental Log interface for outside user.
 * This class provide a way to redirect skity internal log message to outside.
 *  Note: the api may changed in the future.
 *
 *  Example:
 *    skity::Logger::CustomLogger log {};
 *    log.log_i = func_i;
 *    log.log_d = func_d;
 *    log.log_e = func_e;
 *
 *    skity::Log::RegisteLog(&log);
 */
class SKITY_API Logger final {
 public:
  using LogHandler = void (*)(const char*);

  struct CustomLogger {
    LogHandler log_i = nullptr;
    LogHandler log_d = nullptr;
    LogHandler log_e = nullptr;
  };

  Logger() = delete;
  ~Logger() = delete;

  /**
   * @brief     Registe custom log function into Skity. Need to unregiste these
   *            log if outside log context is destroied.
   *
   * @param log Contains custom log function. If pass null, means unregiste
   */
  static void RegisteLog(CustomLogger* log);
};

}  // namespace skity

#endif  // INCLUDE_SKITY_IO_LOGGER_HPP
