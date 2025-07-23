// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GRAPHIC_COLOR_PRIV_NEON_HPP
#define SRC_GRAPHIC_COLOR_PRIV_NEON_HPP

#include <skity/macros.hpp>

#ifndef SKITY_ARM_NEON
#error "not enable arm neon"
#endif

#include <arm_neon.h>

namespace skity {

static inline uint8x8_t MulDiv255RoundNeon(uint8x8_t x, uint8x8_t y) {
  uint16x8_t prod = vmull_u8(x, y);

  return vraddhn_u16(prod, vrshrq_n_u16(prod, 8));
}

static inline uint8x8x4_t PMSrcOverNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  uint8x8_t nalphas = vmvn_u8(src.val[3]);  // 256 - src.alpha

  return {
      vqadd_u8(src.val[0], MulDiv255RoundNeon(nalphas, dst.val[0])),
      vqadd_u8(src.val[1], MulDiv255RoundNeon(nalphas, dst.val[1])),
      vqadd_u8(src.val[2], MulDiv255RoundNeon(nalphas, dst.val[2])),
      vqadd_u8(src.val[3], MulDiv255RoundNeon(nalphas, dst.val[3])),
  };
}

static inline uint8x8x4_t PMSrcInNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  uint8x8_t nalphas = dst.val[3];  // src alpha

  return {
      MulDiv255RoundNeon(nalphas, src.val[0]),
      MulDiv255RoundNeon(nalphas, src.val[1]),
      MulDiv255RoundNeon(nalphas, src.val[2]),
      MulDiv255RoundNeon(nalphas, src.val[3]),
  };
}

static inline uint8x8x4_t PMSrcOutNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  uint8x8_t nalphas = vmvn_u8(dst.val[3]);  // 256 - dst.alpha

  return {
      MulDiv255RoundNeon(nalphas, src.val[0]),
      MulDiv255RoundNeon(nalphas, src.val[1]),
      MulDiv255RoundNeon(nalphas, src.val[2]),
      MulDiv255RoundNeon(nalphas, src.val[3]),
  };
}

// r = s*da + d*(1-sa)
static inline uint8x8x4_t PMSrcATopNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  uint8x8_t one_minus_sa = vmvn_u8(src.val[3]);  // 256 - src.alpha
  uint8x8_t da = dst.val[3];

  return {
      vqadd_u8(MulDiv255RoundNeon(src.val[0], da),
               MulDiv255RoundNeon(dst.val[0], one_minus_sa)),
      vqadd_u8(MulDiv255RoundNeon(src.val[1], da),
               MulDiv255RoundNeon(dst.val[1], one_minus_sa)),
      vqadd_u8(MulDiv255RoundNeon(src.val[2], da),
               MulDiv255RoundNeon(dst.val[2], one_minus_sa)),
      vqadd_u8(MulDiv255RoundNeon(src.val[3], da),
               MulDiv255RoundNeon(dst.val[3], one_minus_sa)),
  };
}

// r = s*(1-da) + d*(1-sa)
static inline uint8x8x4_t PMXorNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  uint8x8_t one_minus_sa = vmvn_u8(src.val[3]);  // 256 - src.alpha
  uint8x8_t one_minus_da = vmvn_u8(dst.val[3]);  // 256 - dst.alpha

  return {
      vqadd_u8(MulDiv255RoundNeon(src.val[0], one_minus_da),
               MulDiv255RoundNeon(dst.val[0], one_minus_sa)),
      vqadd_u8(MulDiv255RoundNeon(src.val[1], one_minus_da),
               MulDiv255RoundNeon(dst.val[1], one_minus_sa)),
      vqadd_u8(MulDiv255RoundNeon(src.val[2], one_minus_da),
               MulDiv255RoundNeon(dst.val[2], one_minus_sa)),
      vqadd_u8(MulDiv255RoundNeon(src.val[3], one_minus_da),
               MulDiv255RoundNeon(dst.val[3], one_minus_sa)),
  };
}

// r = min(s + d, 1)
static inline uint8x8x4_t PMPlusNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  uint8x8_t one = vmov_n_u8(255);

  return {
      vmin_u8(vqadd_u8(src.val[0], dst.val[0]), one),
      vmin_u8(vqadd_u8(src.val[1], dst.val[1]), one),
      vmin_u8(vqadd_u8(src.val[2], dst.val[2]), one),
      vmin_u8(vqadd_u8(src.val[3], dst.val[3]), one),
  };
}

// r = s*d
static inline uint8x8x4_t PMModulateNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  return {
      MulDiv255RoundNeon(src.val[0], dst.val[0]),
      MulDiv255RoundNeon(src.val[1], dst.val[1]),
      MulDiv255RoundNeon(src.val[2], dst.val[2]),
      MulDiv255RoundNeon(src.val[3], dst.val[3]),
  };
}

// r = s + d - s*d
static inline uint8x8x4_t PMScreenNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  return {
      vqsub_u8(vqadd_u8(src.val[0], dst.val[0]),
               MulDiv255RoundNeon(src.val[0], dst.val[0])),
      vqsub_u8(vqadd_u8(src.val[1], dst.val[1]),
               MulDiv255RoundNeon(src.val[1], dst.val[1])),
      vqsub_u8(vqadd_u8(src.val[2], dst.val[2]),
               MulDiv255RoundNeon(src.val[2], dst.val[2])),
      vqsub_u8(vqadd_u8(src.val[3], dst.val[3]),
               MulDiv255RoundNeon(src.val[3], dst.val[3])),
  };
}

}  // namespace skity

#endif  // SRC_GRAPHIC_COLOR_PRIV_NEON_HPP
