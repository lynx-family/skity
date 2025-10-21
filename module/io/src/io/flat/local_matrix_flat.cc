// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "src/io/flat/local_matrix_flat.hpp"

#include "src/io/memory_read.hpp"

namespace skity {

std::string_view LocalMatrixFlat::ProcName() const {
  return "SkLocalMatrixShader";
}

void LocalMatrixFlat::FlattenToBuffer(WriteBuffer& buffer) const {
  buffer.WriteMatrix(matrix_);
  buffer.WriteFlattenable(shader_);
}

std::shared_ptr<Shader> LocalMatrixFlat::ReadFromBuffer(ReadBuffer& buffer) {
  auto matrix = buffer.ReadMatrix();

  if (!buffer.Validate(matrix.has_value())) {
    return {};
  }

  auto shader = buffer.ReadShader();

  if (!buffer.Validate(shader != nullptr)) {
    return {};
  }

  shader->SetLocalMatrix(matrix.value());

  return shader;
}

}  // namespace skity
