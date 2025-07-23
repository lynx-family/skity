// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_TEXT_SCALER_CONTEXT_CACHE_HPP
#define SRC_TEXT_SCALER_CONTEXT_CACHE_HPP

#include <cstdint>
#include <skity/graphic/paint.hpp>
#include <skity/text/font.hpp>
#include <skity/text/typeface.hpp>

#include "src/base/lru_cache.hpp"
#include "src/text/scaler_context_container.hpp"
#include "src/text/scaler_context_desc.hpp"

namespace skity {

class ScalerContextCache final {
 public:
  static ScalerContextCache* GlobalScalerContextCache();

 public:
  ScalerContextCache();
  ~ScalerContextCache() = default;

  std::shared_ptr<ScalerContextContainer> FindOrCreateScalerContext(
      const ScalerContextDesc& desc, Typeface* typeface);

 private:
  std::shared_ptr<ScalerContextContainer> CreateScalerContext(
      const ScalerContextDesc& desc, Typeface* typeface);

  LRUCache<ScalerContextDesc, std::shared_ptr<ScalerContextContainer>> cache_;
  std::mutex mutex_;
};

}  // namespace skity

#endif  // SRC_TEXT_SCALER_CONTEXT_CACHE_HPP
