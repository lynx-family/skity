// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "harness/font/artifact/fingerprint.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace skity {
namespace font_harness {
namespace {

class Sha256State {
 public:
  void Update(std::string_view bytes) {
    size_ += bytes.size();
    for (unsigned char byte : bytes) {
      block_[used_++] = byte;
      if (used_ == 64) {
        Transform();
        used_ = 0;
      }
    }
  }
  std::string Finish() {
    const uint64_t bits = size_ * 8;
    Update(std::string_view("\x80", 1));
    while (used_ != 56) {
      Update(std::string_view("\0", 1));
    }
    for (int i = 7; i >= 0; --i) {
      const char byte = static_cast<char>(bits >> (i * 8));
      Update(std::string_view(&byte, 1));
    }
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (uint32_t word : state_) {
      out << std::setw(8) << word;
    }
    return out.str();
  }

 private:
  static uint32_t Rotate(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
  }
  void Transform() {
    static constexpr uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
      w[i] = uint32_t(block_[4 * i]) << 24 | uint32_t(block_[4 * i + 1]) << 16 |
             uint32_t(block_[4 * i + 2]) << 8 | block_[4 * i + 3];
    }
    for (int i = 16; i < 64; ++i) {
      const auto a = w[i - 15], b = w[i - 2];
      w[i] = w[i - 16] + (Rotate(a, 7) ^ Rotate(a, 18) ^ (a >> 3)) + w[i - 7] +
             (Rotate(b, 17) ^ Rotate(b, 19) ^ (b >> 10));
    }
    auto s = state_;
    for (int i = 0; i < 64; ++i) {
      const auto t1 = s[7] +
                      (Rotate(s[4], 6) ^ Rotate(s[4], 11) ^ Rotate(s[4], 25)) +
                      ((s[4] & s[5]) ^ (~s[4] & s[6])) + k[i] + w[i];
      const auto t2 = (Rotate(s[0], 2) ^ Rotate(s[0], 13) ^ Rotate(s[0], 22)) +
                      ((s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]));
      s = {t1 + t2, s[0], s[1], s[2], s[3] + t1, s[4], s[5], s[6]};
    }
    for (int i = 0; i < 8; ++i) {
      state_[i] += s[i];
    }
  }
  std::array<uint32_t, 8> state_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                    0xa54ff53a, 0x510e527f, 0x9b05688c,
                                    0x1f83d9ab, 0x5be0cd19};
  std::array<unsigned char, 64> block_{};
  size_t used_ = 0;
  uint64_t size_ = 0;
};

void Canonicalize(const Json::Value& v, std::string* out) {
  if (v.isNull()) {
    *out += 'n';
  } else if (v.isBool()) {
    *out += v.asBool() ? 't' : 'f';
  } else if (v.isNumeric()) {
    static_assert(sizeof(double) == 8 &&
                  std::numeric_limits<double>::is_iec559);
    const double number = v.asDouble() == 0 ? 0 : v.asDouble();
    uint64_t bits;
    std::memcpy(&bits, &number, sizeof(bits));
    *out += 'd';
    for (int i = 7; i >= 0; --i) {
      *out += static_cast<char>(bits >> (8 * i));
    }
  } else if (v.isString()) {
    const auto value = v.asString();
    *out += "s" + std::to_string(value.size()) + ":" + value;
  } else if (v.isArray()) {
    *out += "a" + std::to_string(v.size()) + ":";
    for (const auto& item : v) {
      Canonicalize(item, out);
    }
  } else {
    auto keys = v.getMemberNames();
    std::sort(keys.begin(), keys.end());
    *out += "o" + std::to_string(keys.size()) + ":";
    for (const auto& key : keys) {
      Canonicalize(Json::Value(key), out);
      Canonicalize(v[key], out);
    }
  }
}

}  // namespace

std::string Sha256(std::string_view bytes) {
  Sha256State state;
  state.Update(bytes);
  return state.Finish();
}

bool FileSha256(const std::filesystem::path& path, std::string* digest,
                std::string* error) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    *error = "cannot open input: " + path.string();
    return false;
  }
  Sha256State state;
  char buffer[64 * 1024];
  while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
    state.Update(std::string_view(buffer, static_cast<size_t>(file.gcount())));
  }
  if (!file.eof()) {
    *error = "cannot read input: " + path.string();
    return false;
  }
  *digest = state.Finish();
  return true;
}

std::string JsonFingerprint(const Json::Value& value) {
  std::string canonical;
  Canonicalize(value, &canonical);
  return Sha256(canonical);
}

Json::Value BuildInputFingerprint(const CaseValidationResult& input,
                                  ValidationContext* errors) {
  Json::Value result(Json::objectValue);
  result["format"] = "case-json-v1";
  result["case_sha256"] = JsonFingerprint(input.normalized_case);
  result["font_files"] = Json::Value(Json::arrayValue);
  for (const auto& font : input.resolved_font_files) {
    Json::Value file(Json::objectValue);
    file["uri"] = font.uri;
    file["collection_index"] = font.collection_index;
    std::string digest, error;
    if (!FileSha256(font.absolute_path, &digest, &error)) {
      errors->AddError("$.font_files", error);
    }
    file["sha256"] = digest;
    result["font_files"].append(std::move(file));
  }
  return result;
}

}  // namespace font_harness
}  // namespace skity
