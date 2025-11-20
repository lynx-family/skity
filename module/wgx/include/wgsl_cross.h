// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#if defined(WGX_DLL)
#if defined(_MSC_VER)
#define WGX_API __declspec(dllexport)
#else
#define WGX_API __attribute__((visibility("default")))
#endif
#else
#define WGX_API
#endif

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace wgx {

struct Token;

namespace ast {

class NodeAllocator;
struct Module;

}  // namespace ast

struct WGX_API GlslOptions {
  enum class Standard {
    kDesktop,
    kES,
  };

  Standard standard = Standard::kDesktop;
  uint32_t major_version = 3;
  uint32_t minor_version = 3;
};

struct WGX_API MslOptions {
  bool use_arguments_buffer = false;

  uint32_t msl_version_major = 2;
  uint32_t msl_version_minor = 3;

  uint32_t buffer_base_index = 2;
  uint32_t texture_base_index = 0;
  uint32_t sampler_base_index = 0;
};

enum class BindingType {
  kUndefined,
  kUniformBuffer,
  kTexture,
  kSampler,
};

enum ShaderStage : uint32_t {
  ShaderStage_kNone = 0,
  ShaderStage_kVertex = 1 << 0,
  ShaderStage_kFragment = 1 << 1,
};

inline ShaderStage operator|(ShaderStage lhs, ShaderStage rhs) {
  return static_cast<ShaderStage>(static_cast<uint32_t>(lhs) |
                                  static_cast<uint32_t>(rhs));
}

inline ShaderStage& operator|=(ShaderStage& lhs, ShaderStage rhs) {
  return lhs = lhs | rhs;
}

struct WGX_API TypeDefinition {
  // The name of the type. For example, "vec2<f32>" or "array<f32>".
  std::string name = {};
  // The size of the type in bytes.
  size_t size = 0;
  // The alignment of the type in bytes.
  size_t alignment = 0;

  TypeDefinition(std::string_view name, size_t size, size_t align)
      : name(name), size(size), alignment(align) {}

  virtual ~TypeDefinition() = default;

  virtual bool SetData(const void* data, size_t size) = 0;

  virtual void WriteToBuffer(void* buffer, size_t offset) const = 0;

  virtual bool IsArray() const { return false; }

  virtual bool IsStruct() const { return false; }

  template <typename T>
  bool SetData(const T& data) {
    return SetData(&data, sizeof(T));
  }
};

struct WGX_API ArrayDefinition : public TypeDefinition {
  size_t count = 0;

  ArrayDefinition() : TypeDefinition("Array", 0, 0) {}

  ~ArrayDefinition() override = default;

  virtual TypeDefinition* GetElementAt(uint32_t index) = 0;

  bool IsArray() const override { return true; }

  template <typename T>
  bool SetDataAt(uint32_t index, const T& data) {
    if (index >= count) {
      return false;
    }

    return GetElementAt(index)->SetData(data);
  }

  bool SetDataAt(uint32_t index, const void* data, size_t size) {
    if (index >= count) {
      return false;
    }

    return GetElementAt(index)->SetData(data, size);
  }
};

struct WGX_API Field {
  std::string name = {};
  TypeDefinition* type = nullptr;
  size_t offset = 0;

  Field(std::string_view name, TypeDefinition* type) : name(name), type(type) {}

  ~Field() { delete type; }
};

struct WGX_API StructDefinition : public TypeDefinition {
  std::vector<Field*> members = {};

  StructDefinition(const std::string_view& name, std::vector<Field*> members);

  ~StructDefinition() override {
    for (auto* member : members) {
      delete member;
    }
  }

  bool SetData(const void* data, size_t size) override;

  void WriteToBuffer(void* buffer, size_t offset) const override;

  bool IsStruct() const override { return true; }

  Field* GetMember(const std::string_view& name);
};

/**
 * Our binding group entry reflection.
 *
 * It contains the information about the binding group entry in original WGSL
 * source code.
 *
 * Also, it contains the information about the binding group entry in the
 * target shader language. For example:
 *   1. For MSL, it contains the index of the buffer texture or sampler the
 *      compiler actually used.
 *   2. For GLSL, it contains the index of the uniform block the compiler
 *      actually used.
 */
struct WGX_API BindGroupEntry {
  BindingType type = BindingType::kUndefined;

  // The original binding index in WGSL source code.
  uint32_t binding = 0;

  // The variable name of this binding in WGSL source code.
  std::string name = {};

  // The binding index in the target shader language.
  // In MSL, it could be the index of the buffer texture or sampler based on
  // the binding type.
  uint32_t index = 0;

  // The texture units used in the texture binding.
  // This field is only valid for sampler binding in GLSL.
  std::optional<std::vector<uint32_t>> units = std::nullopt;

  ShaderStage stage = ShaderStage_kNone;

  // memory layout information. If the binding type is a uniform buffer
  std::shared_ptr<TypeDefinition> type_definition = {};
};

struct WGX_API BindGroup {
  // The original binding group index in WGSL source code.
  uint32_t group = 0;

  std::vector<BindGroupEntry> entries = {};

  /**
   * Get the entry of the binding group by the binding index.
   *
   * @param binding The binding index in original WGSL code.
   *
   * @return The entry of the binding group.
   *         If the binding index is not found, return nullptr.
   */
  const BindGroupEntry* GetEntry(uint32_t binding) const;
  BindGroupEntry* GetEntry(uint32_t binding);

  /**
   * Merge the bind group entries with another bind group.
   * Some times, if two entry points function share the same bind group.
   * The compiler may generate the same binding index for different entry.
   * In this case, we need to merge the entries of the two bind groups.
   *
   * @param other The other bind group to merge.
   *
   * @return true if the merge is successful.
   *         false if there is some conflict caused by same binding index with
   *         different binding type.
   */
  bool Merge(const BindGroup& other);
};

struct WGX_API CompilerContext {
  uint32_t last_ubo_binding = 0;
  uint32_t last_texture_binding = 0;
  uint32_t last_sampler_binding = 0;
};

struct WGX_API Result {
  std::string content = {};

  std::vector<BindGroup> bind_groups = {};

  bool success = false;

  CompilerContext context = {};

  Result() = default;
  Result(std::string content, std::vector<BindGroup> groups)
      : content(std::move(content)),
        bind_groups(std::move(groups)),
        success(true) {}

  Result(std::string content, std::vector<BindGroup> groups,
         CompilerContext context)
      : content(std::move(content)),
        bind_groups(std::move(groups)),
        success(true),
        context(std::move(context)) {}

  operator bool() const { return success; }
};

struct WGX_API Diagnosis {
  std::string message = {};

  size_t line = 0;
  size_t column = 0;
};

/**
 * The WGSL program.
 *
 * It can parse the WGSL source code and generate the AST.
 * Also, it can generate the binding groups and the memory layout info.
 *
 */
class WGX_API Program {
 public:
  ~Program();

  /**
   * Parse the WGSL source code and generate the AST.
   *
   * @param source The WGSL source code.
   *
   * @return The WGSL program instance or null if there is some
   */
  static std::unique_ptr<Program> Parse(std::string source);

  /**
   * Get the diagnosis of the program.
   * @note This compile will stop when meet the first error.
   *
   * @return The diagnosis of the program.
   */
  std::optional<Diagnosis> GetDiagnosis() const { return mDiagnosis; }

  /**
   * Write the AST to the target OpenGL shader language.
   *
   * @param entry_point The entry point function name. In the WGSL source
   * code.
   * @param options     The options for the OpenGL shader language.
   * @param ctx         The compiler context. It is used to record the binding
   *                    slot used by the compiler.
   *
   * @return            The result of the compilation.
   */
  Result WriteToGlsl(const char* entry_point, const GlslOptions& options,
                     std::optional<CompilerContext> ctx = std::nullopt) const;

  /**
   * Write the AST to the target Metal shader language.
   *
   * @param entry_point The entry point function name. In the WGSL source
   * code.
   * @param options     The options for the Metal shader language.
   * @param ctx         The compiler context. It is used to record the binding
   *                    slot used by the compiler.
   *
   * @return            The result of the compilation.
   */
  Result WriteToMsl(const char* entry_point, const MslOptions& options,
                    std::optional<CompilerContext> ctx = std::nullopt) const;

  /**
   * Get the bind groups of the entry point function from original WGSL source
   * code.
   *
   * @param entry_point The entry point function name. In the WGSL source code.
   *
   * @return The bind groups of the entry point function captured.
   */
  std::vector<BindGroup> GetWGSLBindGroups(const char* entry_point) const;

 private:
  explicit Program(std::string source);

  bool Parse();

  bool BuildAST(const std::vector<Token>& toke_list);

 private:
  ast::NodeAllocator* ast_allocator_;
  std::string mSource = {};
  ast::Module* module_ = nullptr;
  std::optional<Diagnosis> mDiagnosis = std::nullopt;
};

}  // namespace wgx
