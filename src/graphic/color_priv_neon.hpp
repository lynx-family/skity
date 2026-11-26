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

static inline uint8x8x4_t PMColorWeightedSumNeon(uint8x8x4_t a,
                                                 uint8x8_t a_weight,
                                                 uint8x8x4_t b,
                                                 uint8x8_t b_weight) {
  for (int i = 0; i < 4; ++i) {
    uint16x8_t sum = vmlal_u8(vmull_u8(a.val[i], a_weight), b.val[i], b_weight);
    a.val[i] = vraddhn_u16(sum, vrshrq_n_u16(sum, 8));
  }
  return a;
}

// r = s*da + d*(1-sa)
static inline uint8x8x4_t PMSrcATopNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  return PMColorWeightedSumNeon(src, dst.val[3], dst, vmvn_u8(src.val[3]));
}

// r = s*(1-da) + d*(1-sa)
static inline uint8x8x4_t PMXorNeon(uint8x8x4_t dst, uint8x8x4_t src) {
  return PMColorWeightedSumNeon(src, vmvn_u8(dst.val[3]), dst,
                                vmvn_u8(src.val[3]));
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
  // Screen is the complement of multiplying the two complements.
  for (int i = 0; i < 4; ++i) {
    dst.val[i] =
        vmvn_u8(MulDiv255RoundNeon(vmvn_u8(src.val[i]), vmvn_u8(dst.val[i])));
  }
  return dst;
}

}  // namespace skity

#endif  // SRC_GRAPHIC_COLOR_PRIV_NEON_HPP
