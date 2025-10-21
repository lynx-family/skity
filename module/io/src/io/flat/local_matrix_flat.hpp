// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef MODULE_IO_SRC_IO_FLAT_LOCAL_MATRIX_FLAT_HPP
#define MODULE_IO_SRC_IO_FLAT_LOCAL_MATRIX_FLAT_HPP

#include <skity/effect/shader.hpp>

namespace skity {

class ReadBuffer;

/**
 * If shader contains local matrix, skia will create a LocalMatrixShader wrap
 * the shader and matrix.
 *
 * Skity does not have LocalMatrixShader, so use this class is used to mock the
 * LocalMatrixShader in new skia version.
 *
 */
class LocalMatrixFlat : public Flattenable {
 public:
  LocalMatrixFlat(const Matrix& matrix, const Shader* shader)
      : matrix_(matrix), shader_(shader) {}
  ~LocalMatrixFlat() override = default;

  std::string_view ProcName() const override;

  void FlattenToBuffer(WriteBuffer& buffer) const override;

  static std::shared_ptr<Shader> ReadFromBuffer(ReadBuffer& buffer);

 private:
  Matrix matrix_;
  const Shader* shader_;
};

}  // namespace skity

#endif  // MODULE_IO_SRC_IO_FLAT_LOCAL_MATRIX_FLAT_HPP
