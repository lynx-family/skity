// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef SRC_GPU_GPU_SHADER_FUNCTION_HPP
#define SRC_GPU_GPU_SHADER_FUNCTION_HPP

#include <wgsl_cross.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace skity {

typedef std::function<void(char const*)> GPUShaderFunctionErrorCallback;

using GPUShaderStageMask = uint32_t;

enum class GPUShaderStage : GPUShaderStageMask {
  kVertex = 0x01,
  kFragment = 0x02
};

enum class GPUShaderSourceType {
  kRaw,
  kWGX,
};

using GPULabelIdToNameProc = std::string (*)(uint64_t);

struct GPULabel {
  explicit constexpr GPULabel(uint64_t id,
                              GPULabelIdToNameProc id_to_name_proc = nullptr)
      : id(id), id_to_name_proc(id_to_name_proc) {}
  explicit GPULabel(std::string label) : label(std::move(label)) {}
  constexpr GPULabel() : id(0u) {}

  const std::string& ToString() const {
    if (!label.has_value()) {
      if (id_to_name_proc != nullptr) {
        label = id_to_name_proc(id);
      } else {
        label = std::to_string(id);
      }
    }
    return label.value();
  }

 private:
  uint64_t id;
  GPULabelIdToNameProc id_to_name_proc = nullptr;
  mutable std::optional<std::string> label = std::nullopt;
};

struct GPUShaderFunctionDescriptor {
  GPULabel label;
  GPUShaderStage stage = GPUShaderStage::kVertex;
  std::vector<int32_t> constant_values = {};
  GPUShaderFunctionErrorCallback error_callback = {};

  GPUShaderSourceType source_type = GPUShaderSourceType::kRaw;
  void* shader_source = nullptr;
};

struct GPUShaderSourceRaw {
  const char* source = nullptr;
  const char* entry_point = nullptr;
};

class GPUShaderFunction {
 public:
  explicit GPUShaderFunction(GPULabel label) : label_(std::move(label)) {}

  virtual ~GPUShaderFunction() = default;
  virtual bool IsValid() const = 0;

  const std::string GetLabel() const { return label_.ToString(); }

  const std::vector<wgx::BindGroup>& GetBindGroups() const {
    return bind_groups_;
  }

  void SetBindGroups(std::vector<wgx::BindGroup> bind_groups) {
    bind_groups_ = bind_groups;
  }

  const wgx::CompilerContext& GetWGXContext() const { return wgx_context_; }

  void SetWGXContext(wgx::CompilerContext wgx_context) {
    wgx_context_ = wgx_context;
  }

 private:
  GPULabel label_ = {};

  // The wgx context infor for this shader function.
  // The context contains the uniform buffer slot and texture slot used in this
  // shader function.
  wgx::CompilerContext wgx_context_ = {};

  // The bind groups of the shader function. In skity the shader function may
  // only contain one bind group. But for compatibility with WGX, we still keep
  // the bind groups as a vector.
  std::vector<wgx::BindGroup> bind_groups_ = {};
};

}  // namespace skity

#endif  // SRC_GPU_GPU_SHADER_FUNCTION_HPP
