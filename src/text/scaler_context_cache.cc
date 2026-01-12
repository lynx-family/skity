// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/text/scaler_context_cache.hpp"

#include "src/utils/no_destructor.hpp"

namespace skity {

constexpr size_t kMaxCacheSize = 2048;

ScalerContextCache* ScalerContextCache::GlobalScalerContextCache() {
  static NoDestructor<ScalerContextCache> cache;
  return cache.get();
}

ScalerContextCache::ScalerContextCache() : cache_(kMaxCacheSize) {}

std::shared_ptr<ScalerContextContainer>
ScalerContextCache::FindOrCreateScalerContext(
    const ScalerContextDesc& desc, const std::shared_ptr<Typeface>& typeface) {
  std::unique_lock<std::mutex> lock(mutex_);
  auto p_scaler_context = cache_.Find(desc);
  if (p_scaler_context) {
    return *p_scaler_context;
  }
  std::shared_ptr<ScalerContextContainer> scaler_context =
      this->CreateScalerContext(desc, typeface);
  cache_.Insert(desc, scaler_context);
  return scaler_context;
}

void ScalerContextCache::PurgeByTypeface(TypefaceID typeface_id) {
  std::unique_lock<std::mutex> lock(mutex_);

  std::vector<ScalerContextDesc> keys = cache_.CollectKeys();
  for (auto& key : keys) {
    if (key.typeface_id == typeface_id) {
      cache_.Remove(key);
    }
  }
}

std::shared_ptr<ScalerContextContainer> ScalerContextCache::CreateScalerContext(
    const ScalerContextDesc& desc, const std::shared_ptr<Typeface>& typeface) {
  auto scaler_context = typeface->CreateScalerContext(&desc);
  return std::make_shared<ScalerContextContainer>(std::move(scaler_context));
}

}  // namespace skity
