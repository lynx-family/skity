// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <array>
#include <cstring>
#include <skity/geometry/matrix.hpp>

#include "src/io/memory_read.hpp"
#include "src/io/memory_writer.hpp"

namespace skity {

static constexpr int kMatrix3x3Size = 9 * sizeof(float);

namespace {

std::array<float, 9> MatrixTo3x3(const Matrix& matrix) {
  return std::array<float, 9>{
      matrix.GetScaleX(), matrix.GetSkewX(),  matrix.GetTranslateX(),
      matrix.GetSkewY(),  matrix.GetScaleY(), matrix.GetTranslateY(),
      matrix.GetPersp0(), matrix.GetPersp1(), matrix.GetPersp2(),
  };
}

Matrix MatrixFrom3x3(const std::array<float, 9>& matrix3x3) {
  Matrix m;

  m.SetScaleX(matrix3x3[0]);
  m.SetSkewX(matrix3x3[1]);
  m.SetTranslateX(matrix3x3[2]);

  m.SetSkewY(matrix3x3[3]);
  m.SetScaleY(matrix3x3[4]);
  m.SetTranslateY(matrix3x3[5]);

  m.SetPersp0(matrix3x3[6]);
  m.SetPersp1(matrix3x3[7]);
  m.SetPersp2(matrix3x3[8]);

  return m;
}

}  // namespace

template <>
void FlatIntoMemory(const Matrix& matrix, MemoryWriter32& writer) {
  auto matrix3x3 = MatrixTo3x3(matrix);

  auto ptr = writer.Reserve(kMatrix3x3Size);

  std::memcpy(ptr, matrix3x3.data(), kMatrix3x3Size);
}

template <>
std::optional<Matrix> ReadFromMemory(ReadBuffer& buffer) {
  if (!buffer.Validate(buffer.IsAvailable(kMatrix3x3Size))) {
    return std::nullopt;
  }

  std::array<float, 9> matrix3x3;

  auto ptr = buffer.Skip(kMatrix3x3Size);

  if (ptr == nullptr) {
    return std::nullopt;
  }

  std::memcpy(matrix3x3.data(), ptr, kMatrix3x3Size);

  return {MatrixFrom3x3(matrix3x3)};
}

}  // namespace skity
