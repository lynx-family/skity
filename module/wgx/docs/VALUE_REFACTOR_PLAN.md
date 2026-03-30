# IR Value Model Refactoring Implementation Plan

## Goal

Replace the current scattered value representation with a unified `Value` / `ExprResult` model.

## Phase 1: Add New Types (Without Breaking Existing Code)

### 1.1 Add `ir/value.h`

- Add `Value`, `ExprResult`, `ConstantPool` definitions
- Do not modify any existing code
- Add basic unit tests to verify new type behaviors

### 1.2 Add Helper Functions

Add conversion helper functions in lowering to prepare for migration:

```cpp
/**
 * Internal helpers in lower_to_ir.cpp
 */

/** Convert old Instruction-based result to new Value */
Value ToValue(const ir::Instruction& inst);

/** Check if Instruction represents a constant */
bool IsConstant(const ir::Instruction& inst);

/** Check if Instruction represents a variable reference */
bool IsVariableRef(const ir::Instruction& inst);
```

## Phase 2: Gradual Lowering Migration

### 2.1 Refactor LowerExpression Return Type

From:
```cpp
bool LowerExpression(ast::Expression*, ir::Instruction* out);
```

To:
```cpp
ExprResult LowerExpression(ast::Expression*);
```

**Steps:**
1. Add new `LowerExpressionNew` function returning `ExprResult`
2. Reuse existing logic internally, but convert to `ExprResult` on return
3. Original `LowerExpression` calls the new function and converts back to old format
4. Replace call sites one by one
5. Remove old function

### 2.2 Introduce Explicit kLoad Instruction

**Current Problem:** Variable reads are handled implicitly

**Goal:** Lowering explicitly generates kLoad when encountering variables as rvalues

```cpp
Value EnsureValue(const ExprResult& expr, Block* block) {
  if (!expr.IsAddress()) {
    return expr.value;
  }
  /** Generate explicit load */
  uint32_t load_id = AllocateSSAId();
  block->instructions.push_back(
    Instruction::Load(load_id, expr.GetType(), expr.value)
  );
  return Value::SSA(expr.GetType(), load_id);
}
```

### 2.3 Refactor LowerBinaryExpression

**Current:** Directly generates kBinary instruction, operands may be variable id or SSA id

**Goal:** Operands uniformly use Value, variable references are loaded first

```cpp
/**
 * Before:
 * binary_inst.operands.push_back(ir::Operand::Id(lhs_inst.value_id));
 * or
 * binary_inst.operands.push_back(ir::Operand::Id(lhs_inst.var_id));
 *
 * After:
 */
Value lhs_val = EnsureValue(lhs_result, block);
Value rhs_val = EnsureValue(rhs_result, block);
binary_inst.operands.push_back(lhs_val);
binary_inst.operands.push_back(rhs_val);
```

## Phase 3: Refactor Instruction Structure

### 3.1 Simplify Instruction Definition

**Maintain both Instruction definitions in parallel:**

```cpp
/** Old definition (temporarily kept) */
struct InstructionLegacy { ... };

/** New definition */
struct Instruction {
  InstKind kind;
  uint32_t result_id;
  TypeId result_type;
  std::vector<Value> operands;
  /** ... other specific fields */
};
```

### 3.2 Conversion Layer

Add conversion functions for compatibility:

```cpp
/** Convert new Instruction to old (for existing emitter) */
InstructionLegacy ConvertToLegacy(const Instruction& inst);

/** Or convert old to new (if modifying emitter first) */
Instruction ConvertFromLegacy(const InstructionLegacy& inst);
```

## Phase 4: Refactor Emitter

### 4.1 Unified Value Handling

Replace scattered handling logic:

```cpp
/** Replace these specialized functions: */
bool EmitConstVec4Return(const std::array<float,4>&);
bool EmitVariableRefReturn(uint32_t var_id);
bool EmitValueRefReturn(uint32_t value_id);

/** With unified: */
uint32_t EmitValue(const Value& value);
```

### 4.2 Use Type-Driven Emission

Replace hardcoded `vec4_type_id_`:

```cpp
/**
 * Before:
 * AppendInstruction(&sections_->functions, SpvOpFAdd,
 *                   {vec4_type_id_, result_id, lhs_id, rhs_id});
 *
 * After:
 */
uint32_t spirv_type_id = type_emitter_->EmitType(value.type);
AppendInstruction(&sections_->functions, SpvOpFAdd,
                  {spirv_type_id, result_id, lhs_id, rhs_id});
```

## Phase 5: Cleanup and Verification

### 5.1 Remove Old Code

- Remove `InstructionLegacy`
- Remove `ReturnValueKind` enum
- Remove `const_vec4_f32`, `var_id`, `value_id` fields
- Remove conversion helper functions

### 5.2 Verification

Run full test suite:

```bash
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```

## File Modification Checklist

| File | Change Type | Description |
|------|-------------|-------------|
| `ir/value.h` | Add | New Value model definition |
| `ir/inst_new.h` | Add | Simplified Instruction design (reference) |
| `ir/module.h` | Modify | Eventually replace Instruction definition |
| `lower/lower_to_ir.cpp` | Modify | Use ExprResult |
| `spirv/emitter.cpp` | Modify | Unified Value handling |
| `spirv/emitter.h` | No change | Interface remains unchanged |

## Testing Strategy

1. **Phase 1-2:** All existing tests should pass (using compatibility layer)
2. **Phase 3-4:** Verify each test case individually
3. **Phase 5:** Full test suite + spirv-val verification

## Risk Mitigation

1. **Maintain compilation:** Each commit must compile
2. **Maintain test pass:** Tests must pass at the end of each phase
3. **Backup plan:** Can fall back to compatibility layer if issues arise
4. **Incremental review:** Each phase as separate PR/commit

## Timeline Estimate

| Phase | Estimated Time |
|-------|----------------|
| 1.1: Add ir/value.h | 2 hours |
| 1.2: Helper functions | 2 hours |
| 2.1: LowerExpression refactor | 4 hours |
| 2.2: kLoad explicit | 3 hours |
| 2.3: Binary refactor | 2 hours |
| 3.x: Instruction simplification | 4 hours |
| 4.x: Emitter refactor | 6 hours |
| 5.x: Cleanup and verification | 3 hours |
| **Total** | **~26 hours** |
