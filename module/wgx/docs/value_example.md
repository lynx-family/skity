# Value Model Refactoring Examples

## 1. Lowering Layer Changes

### Current Code (lower_to_ir.cpp)

```cpp
/**
 * Lowering expression returns Instruction* - confusing!
 */
bool LowerExpression(ast::Expression* expr, ir::Instruction* out_inst) {
  /** ... various cases */
  
  /**
   * Constant case - fills const_vec4_f32
   */
  if (LowerVec4F32Const(expr, &values)) {
    out_inst->result_type = vec4_type;
    out_inst->const_vec4_f32 = values;  /** Special field for this case */
    return true;
  }
  
  /**
   * Variable reference case - fills var_id
   */
  if (expr->GetType() == ast::ExpressionType::kIdentifier) {
    out_inst->result_type = vec4_type;
    out_inst->var_id = LookupVar(name);  /** Different field! */
    return true;
  }
  
  /**
   * Binary case - fills value_id and result_id
   */
  if (expr->GetType() == ast::ExpressionType::kBinaryExp) {
    out_inst->kind = ir::InstKind::kBinary;
    out_inst->result_id = AllocateValueId();
    out_inst->value_id = out_inst->result_id;  /** Redundant! */
    out_inst->result_type = vec4_type;
    /** ... more special handling */
    return true;
  }
}
```

### New Code

```cpp
/**
 * Expression lowering returns ExprResult - clear semantics
 */
ExprResult LowerExpression(ast::Expression* expr) {
  /**
   * Constant case - returns Value with Constant kind
   */
  if (auto values = TryLowerVec4F32Const(expr)) {
    return ExprResult::ValueResult(
      Value::ConstantVec4F32(vec4_type, *values)
    );
  }
  
  /**
   * Variable reference case - returns Value with Variable kind (address)
   */
  if (expr->GetType() == ast::ExpressionType::kIdentifier) {
    uint32_t var_id = LookupVar(name);
    /** This is an address (lvalue), not a value! */
    return ExprResult::AddressResult(
      Value::Variable(vec4_type, var_id)
    );
  }
  
  /**
   * Binary case - operands are Values, result is SSA Value
   */
  if (expr->GetType() == ast::ExpressionType::kBinaryExp) {
    ExprResult lhs = LowerExpression(binary->lhs);
    ExprResult rhs = LowerExpression(binary->rhs);
    
    /**
     * If operands are addresses, emit load instructions
     */
    Value lhs_val = EnsureValue(lhs);
    Value rhs_val = EnsureValue(rhs);
    
    uint32_t result_id = AllocateSSAId();
    Instruction bin_inst = Instruction::Binary(
      result_id, vec4_type, op, lhs_val, rhs_val
    );
    current_block_->instructions.push_back(bin_inst);
    
    return ExprResult::ValueResult(Value::SSA(vec4_type, result_id));
  }
}

/**
 * Helper: if operand is address, emit load and return SSA value
 */
Value EnsureValue(const ExprResult& expr) {
  if (!expr.IsAddress()) {
    return expr.value;
  }
  /** Emit load */
  uint32_t load_id = AllocateSSAId();
  Instruction load = Instruction::Load(
    load_id, expr.GetType(), expr.value
  );
  current_block_->instructions.push_back(load);
  return Value::SSA(expr.GetType(), load_id);
}
```

## 2. Return Statement Lowering

### Current Code

```cpp
bool LowerReturnStatement(const ast::ReturnStatement* ret, ir::Block* block) {
  ir::Instruction inst;
  inst.kind = ir::InstKind::kReturn;
  inst.has_return_value = ret->value != nullptr;
  
  if (ret->value != nullptr) {
    ir::Instruction value_inst;
    if (!LowerExpression(ret->value, &value_inst)) return false;
    
    /** Check type against hardcoded vec4 */
    if (value_inst.result_type != vec4_type) return false;
    
    /**
     * Different paths based on what's in value_inst!
     */
    if (value_inst.value_id != 0) {
      inst.return_value_kind = ir::ReturnValueKind::kValueRef;
      inst.value_id = value_inst.value_id;
    } else if (value_inst.var_id != 0) {
      inst.return_value_kind = ir::ReturnValueKind::kVariableRef;
      inst.var_id = value_inst.var_id;
    } else {
      inst.const_vec4_f32 = value_inst.const_vec4_f32;
      inst.return_value_kind = ir::ReturnValueKind::kConstVec4F32;
    }
  }
  
  block->instructions.emplace_back(inst);
  return true;
}
```

### New Code

```cpp
bool LowerReturnStatement(const ast::ReturnStatement* ret, ir::Block* block) {
  if (ret->value == nullptr) {
    block->instructions.push_back(Instruction::ReturnVoid());
    return true;
  }
  
  ExprResult expr = LowerExpression(ret->value);
  if (!expr.IsValid()) return false;
  
  /**
   * Ensure we have a value (not address)
   */
  Value return_value = EnsureValue(expr);
  if (return_value.type != vec4_type) return false;
  
  /**
   * Single unified path - Value handles the details
   */
  block->instructions.push_back(Instruction::ReturnValue(return_value));
  return true;
}
```

## 3. Emitter Changes

### Current Code (emitter.cpp)

```cpp
bool EmitReturn(const ir::Instruction& inst) {
  if (!inst.has_return_value) {
    AppendInstruction(&sections_->functions, SpvOpReturn, {});
    return true;
  }
  
  /**
   * Different paths for different return value kinds!
   */
  switch (inst.return_value_kind) {
    case ir::ReturnValueKind::kConstVec4F32:
      return EmitConstVec4Return(inst.const_vec4_f32);
    case ir::ReturnValueKind::kVariableRef:
      return EmitVariableRefReturn(inst.var_id);
    case ir::ReturnValueKind::kValueRef:
      return EmitValueRefReturn(inst.value_id);
  }
}

bool MaterializeValueOperand(const ir::Operand& operand, uint32_t* value_id) {
  /** Complex probing logic */
  auto value_map_it = value_map_.find(operand.id);
  if (value_map_it != value_map_.end()) {
    *value_id = value_map_it->second;
    return true;
  }
  
  auto var_it = FindLocalVar(operand.id);
  if (var_it != local_vars_.end()) {
    *value_id = ids_.Allocate();
    AppendInstruction(&sections_->functions, SpvOpLoad,
                      {vec4_type_id_, *value_id, var_it->spirv_var_id});
    return true;
  }
  return false;
}
```

### New Code

```cpp
bool EmitReturn(const ir::Instruction& inst) {
  const Value* ret_val = inst.GetReturnValue();
  if (ret_val == nullptr) {
    AppendInstruction(&sections_->functions, SpvOpReturn, {});
    return true;
  }
  
  /**
   * Single unified path
   */
  uint32_t spirv_value_id = EmitValue(*ret_val);
  
  /**
   * Store to position output (for vertex shaders)
   */
  if (has_position_return_) {
    AppendInstruction(&sections_->functions, SpvOpStore,
                      {position_output_var_id_, spirv_value_id});
  }
  AppendInstruction(&sections_->functions, SpvOpReturn, {});
  return true;
}

/**
 * Unified value emission - handles all Value kinds
 */
uint32_t EmitValue(const ir::Value& value) {
  switch (value.kind) {
    case ir::ValueKind::kConstant:
      return EmitConstant(value);
    case ir::ValueKind::kSSA:
      /** Already have SPIR-V id from previous instruction */
      return value_map_.at(value.GetSSAId().value());
    case ir::ValueKind::kVariable:
      /** Emit load */
      return EmitLoadVariable(value);
  }
}

uint32_t EmitConstant(const ir::Value& value) {
  if (value.IsInlineConstant()) {
    switch (value.const_kind) {
      case ir::InlineConstKind::kVec4F32:
        return EmitInlineVec4F32(value.GetVec4F32());
      case ir::InlineConstKind::kF32:
        return type_emitter_->EmitF32Constant(value.GetF32());
      /** ... other inline types */
    }
  } else {
    /** Pool reference */
    return EmitPoolConstant(value.GetPoolIndex().value());
  }
}
```

## 4. Store Instruction Changes

### Current Code

```cpp
bool LowerStoreValue(const ir::Instruction& value_inst, uint32_t var_id,
                     ir::Block* block) {
  ir::Instruction store_inst;
  store_inst.kind = ir::InstKind::kStore;
  store_inst.operands.push_back(ir::Operand::Id(var_id));
  
  /**
   * Different encoding based on value type!
   */
  if (value_inst.value_id != 0) {
    store_inst.operands.push_back(ir::Operand::Id(value_inst.value_id));
  } else if (value_inst.var_id != 0) {
    store_inst.operands.push_back(ir::Operand::Id(value_inst.var_id));
  } else {
    /**
     * Constants expanded to 4 operands!
     */
    for (size_t i = 0; i < 4; ++i) {
      store_inst.operands.push_back(
        ir::Operand::ConstF32(value_inst.const_vec4_f32[i])
      );
    }
  }
  block->instructions.emplace_back(store_inst);
}
```

### New Code

```cpp
bool LowerAssignStatement(const ast::AssignStatement* assign,
                          ir::Block* block) {
  /**
   * Target must be an address
   */
  ExprResult target = LowerExpression(assign->lhs);
  if (!target.IsAddress()) return false;
  
  /**
   * Source can be any expression
   */
  ExprResult source = LowerExpression(assign->rhs);
  Value source_value = EnsureValue(source);
  
  block->instructions.push_back(
    Instruction::Store(target.value, source_value)
  );
  return true;
}

/**
 * In emitter:
 */
bool EmitStore(const ir::Instruction& inst) {
  const Value& target = inst.GetStoreTarget();
  const Value& source = inst.GetStoreValue();
  
  uint32_t spirv_target = var_map_.at(target.GetVarId().value());
  uint32_t spirv_source = EmitValue(source);
  
  AppendInstruction(&sections_->functions, SpvOpStore,
                    {spirv_target, spirv_source});
  return true;
}
```

## Summary: Improvements

| Aspect | Before | After |
|--------|--------|-------|
| **Value representation** | Scattered across multiple fields (var_id, value_id, const_vec4_f32) | Unified `Value` type, discriminated by `kind` |
| **Expression lowering** | Returns `Instruction*`, confusing temporary carrier with actual instruction | Returns `ExprResult`, clearly distinguishes address vs value |
| **Load semantics** | Implicitly handled in emitter | Explicit `kLoad` instruction, lowering generates it |
| **Return** | 3 different `ReturnValueKind` values | Single `Value` operand |
| **Store** | Variable operand count, meaning depends on shape | Fixed 2 `Value` operands (target, source) |
| **Type information** | External lookup needed in some places | `Value` always carries `TypeId` |
