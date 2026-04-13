// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ir/module.h"
#include "spirv/unified1/spirv.h"

namespace wgx {
namespace spirv {

constexpr uint32_t kSpirvVersion13 = 0x00010300u;

// Shared helpers used by the split SPIR-V backend implementation.
std::vector<uint32_t> EncodeStringLiteral(std::string_view value);
void AppendInstruction(std::vector<uint32_t>* words, SpvOp opcode,
                       std::initializer_list<uint32_t> operands);
void AppendInstruction(std::vector<uint32_t>* words, SpvOp opcode,
                       const std::vector<uint32_t>& operands);
void AppendEntryPoint(std::vector<uint32_t>* words, SpvExecutionModel model,
                      uint32_t function_id, std::string_view entry_point,
                      const std::vector<uint32_t>& interfaces = {});
void AppendName(std::vector<uint32_t>* words, uint32_t target_id,
                std::string_view name);
void AppendSection(std::vector<uint32_t>* dst,
                   const std::vector<uint32_t>& src);
SpvExecutionModel ToExecutionModel(ir::PipelineStage stage);
SpvStorageClass ToSpvStorageClass(ir::StorageClass storage);
bool SupportsCurrentIR(const ir::Function& function);
uint32_t FloatToBits(float value);
const ir::Function* FindEntryFunction(const ir::Module& module);

class IdAllocator {
 public:
  uint32_t Allocate() { return next_id_++; }
  uint32_t Bound() const { return next_id_; }

 private:
  uint32_t next_id_ = 1u;
};

struct SectionBuffers {
  // Physical module sections are accumulated separately and stitched together
  // only at the end so helpers can emit in logical order without caring about
  // final binary layout.
  std::vector<uint32_t> capabilities;
  std::vector<uint32_t> memory_model;
  std::vector<uint32_t> entry_points;
  std::vector<uint32_t> execution_modes;
  std::vector<uint32_t> debug;
  std::vector<uint32_t> annotations;
  std::vector<uint32_t> types_consts_globals;
  std::vector<uint32_t> functions;
};

class TypeEmitter {
 public:
  TypeEmitter(IdAllocator* ids, SectionBuffers* sections,
              ir::TypeTable* type_table);

  ir::TypeTable* GetTypeTable() { return type_table_; }
  IdAllocator* GetIds() { return ids_; }

  uint32_t EmitType(ir::TypeId type_id);
  uint32_t GetFunctionTypeVoid();
  uint32_t GetFunctionType(ir::TypeId return_type,
                           const std::vector<ir::TypeId>& param_types);
  uint32_t GetPointerType(ir::TypeId pointee, SpvStorageClass storage);
  uint32_t EmitF32Constant(float value);
  uint32_t EmitI32Constant(int32_t value);
  uint32_t EmitU32Constant(uint32_t value);
  uint32_t EmitBoolConstant(bool value);

 private:
  // Type emission is memoized because the builder asks for the same SPIR-V
  // types/pointers/constants from many different instruction paths.
  void EmitVectorType(const ir::Type* type, uint32_t spirv_id);
  void EmitMatrixType(const ir::Type* type, uint32_t spirv_id);
  void EmitArrayType(const ir::Type* type, uint32_t spirv_id);
  void EmitStructType(const ir::Type* type, uint32_t spirv_id);
  void EmitPointerType(const ir::Type* type, uint32_t spirv_id);

  struct PairHash {
    size_t operator()(const std::pair<ir::TypeId, SpvStorageClass>& p) const;
  };

  struct FunctionSignature {
    ir::TypeId return_type = ir::kInvalidTypeId;
    std::vector<ir::TypeId> param_types;

    bool operator==(const FunctionSignature& other) const {
      return return_type == other.return_type &&
             param_types == other.param_types;
    }
  };

  struct FunctionSignatureHash {
    size_t operator()(const FunctionSignature& sig) const;
  };

  IdAllocator* ids_;
  SectionBuffers* sections_;
  ir::TypeTable* type_table_;
  std::unordered_map<ir::TypeId, uint32_t> emitted_types_;
  std::unordered_map<uint32_t, uint32_t> f32_constants_;
  std::unordered_map<int32_t, uint32_t> i32_constants_;
  std::unordered_map<uint32_t, uint32_t> u32_constants_;
  std::unordered_map<bool, uint32_t> bool_constants_;
  std::unordered_map<std::pair<ir::TypeId, SpvStorageClass>, uint32_t, PairHash>
      pointer_types_;
  std::unordered_map<FunctionSignature, uint32_t, FunctionSignatureHash>
      function_types_;
  uint32_t function_void_type_ = 0;
};

struct LocalVarInfo {
  uint32_t ir_var_id = 0;
  ir::TypeId var_type = ir::kInvalidTypeId;
  uint32_t spirv_var_id = 0;
};

struct GlobalVarInfo {
  uint32_t ir_var_id = 0;
  ir::TypeId var_type = ir::kInvalidTypeId;
  ir::TypeId inner_type = ir::kInvalidTypeId;
  uint32_t spirv_var_id = 0;
  SpvStorageClass storage_class = SpvStorageClassPrivate;
  std::optional<ir::Value> initializer;
  std::optional<uint32_t> group;
  std::optional<uint32_t> binding;
  bool is_wrapped_buffer = false;
};

struct ValueInfo {
  uint32_t ir_value_id = 0;
  ir::TypeId value_type = ir::kInvalidTypeId;
};

struct FunctionParamInfo {
  uint32_t ir_var_id = 0;
  ir::TypeId var_type = ir::kInvalidTypeId;
  std::string name;
  uint32_t spirv_param_id = 0;
  uint32_t spirv_local_id = 0;
};

class ModuleBuilder {
 public:
  ModuleBuilder(const ir::Module& module, const ir::Function& entry,
                SpvExecutionModel execution_model);

  bool Build(SectionBuffers* sections, std::vector<uint32_t>* output_words);

 private:
  struct OutputVarInfo {
    std::string name;
    ir::TypeId ir_type = ir::kInvalidTypeId;
    ir::OutputDecorationKind decoration_kind = ir::OutputDecorationKind::kNone;
    uint32_t decoration_value = 0;
    uint32_t spirv_var_id = 0;

    ir::BuiltinType GetBuiltin() const;
    uint32_t GetLocation() const;
  };

  // Global variables are discovered from IR operands rather than declarations,
  // because globals are module-level metadata instead of kVariable
  // instructions inside function blocks.
  void CollectGlobalVarReferences(const ir::Function& function);
  bool AllocateCoreIds();
  void WriteCapabilityMemoryModel();
  void WriteEntryPointSection();
  void WriteExecutionModeSection();
  void WriteDebugSection();
  void WriteAnnotationSection();
  bool WriteTypeConstGlobalSection();
  bool WriteFunctionSection();
  bool AnalyzeFunction(const ir::Function& function);
  bool EmitInstruction(const ir::Instruction& inst);
  uint32_t FindVariableSpirvId(uint32_t ir_var_id);
  const ir::Function* FindFunctionByName(std::string_view name) const;
  const GlobalVarInfo* FindGlobalVarInfo(uint32_t ir_var_id) const;
  uint32_t GetAccessPointer(uint32_t ir_var_id, ir::TypeId inner_type,
                            uint32_t* out_ptr_id);
  bool EmitLoad(const ir::Instruction& inst);
  bool EmitStore(const ir::Instruction& inst);
  bool EmitBinary(const ir::Instruction& inst);
  bool EmitConstruct(const ir::Instruction& inst);
  bool EmitCall(const ir::Instruction& inst);
  bool MaterializeValue(const ir::Value& value, uint32_t* value_id);
  uint32_t GetSpirvTypeId(ir::TypeId type_id);
  uint32_t EmitConstant(const ir::Value& value);
  uint32_t EmitConstantComposite(const ir::Value& value);
  uint32_t EmitWrappedConstant(const ir::Value& inner_value,
                               ir::TypeId struct_type_id);
  bool EmitBranch(const ir::Instruction& inst);
  bool EmitCondBranch(const ir::Instruction& inst);
  bool EmitReturn(const ir::Instruction& inst);
  uint32_t GetFunctionId(std::string_view function_name) const;
  uint32_t GetFunctionTypeId(std::string_view function_name) const;
  ir::TypeId GetSpirvFunctionReturnType(const ir::Function& function) const;
  void AssembleModule(std::vector<uint32_t>* output_words);
  std::vector<LocalVarInfo>::iterator FindLocalVar(uint32_t ir_var_id);
  std::vector<ValueInfo>::iterator FindValue(uint32_t ir_value_id);
  uint32_t GetOrCreateBlockLabel(ir::BlockId block_id);

  const ir::Module& module_;
  const ir::Function& entry_;
  SpvExecutionModel execution_model_;

  IdAllocator ids_;
  SectionBuffers* sections_ = nullptr;
  std::unique_ptr<TypeEmitter> type_emitter_;

  // Function-local emission state.
  uint32_t function_id_ = 0;
  uint32_t function_type_id_ = 0;
  std::unordered_map<std::string, uint32_t> function_id_map_;
  std::unordered_map<std::string, uint32_t> function_type_id_map_;
  std::unordered_map<ir::BlockId, uint32_t> block_label_map_;
  std::vector<OutputVarInfo> output_vars_;
  std::vector<FunctionParamInfo> function_params_;
  std::vector<LocalVarInfo> local_vars_;
  std::vector<GlobalVarInfo> global_vars_;
  std::vector<ValueInfo> values_;
  std::unordered_map<uint32_t, uint32_t> value_map_;
  const ir::Block* current_block_ = nullptr;
};

}  // namespace spirv
}  // namespace wgx
