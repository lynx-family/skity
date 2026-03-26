# WGX IR/SPIR-V Dev Log

This file records incremental development progress for the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline and the next
implementation steps.

## 2026-03-26

### Summary

Completed Phase A foundation work: implemented extensible type system (TypeTable),
OOP-style Lowerer refactoring, and variable declaration/load/store support.

### Completed

1. **Implemented TypeTable for extensible type system (Phase A).**
   - **Files**: `module/wgx/ir/type.h`, `module/wgx/ir/type.cpp`
   - **Design**:
     - Replaced `ValueType` enum with `TypeId` (uint32_t) + `TypeTable` design
     - Module-level TypeTable: Types are shared across all functions in a module
     - Type deduplication via hash map (`unordered_map<Type, TypeId>`)
     - 1-based TypeId (0 reserved for `kInvalidTypeId`)
   - **Supported types**:
     - Scalar: `void`, `bool`, `i32`, `u32`, `f32`
     - Vector: `vec2/3/4<T>` (parametric)
     - Matrix: `matRxC<T>` (parametric)
     - Array: `array<T, N>`
     - Struct: user-defined with named members
     - Pointer: `ptr<storage_class, T>`
   - **Storage classes**: Function, Private, Uniform, Storage, Input, Output, Workgroup, Handle
   - **SPIR-V integration**: Added `TypeEmitter` class for IR-to-SPIR-V type emission
   - **Build**: Added to `module/wgx/CMakeLists.txt`

2. **Lowerer refactored to OOP design.**
   - **File**: `module/wgx/lower/lower_to_ir.cpp`
   - **Changes**:
     - Converted from C-style free functions to `Lowerer` class
     - Encapsulates AST module, entry point, IR function as member variables
     - Private methods: `LowerStatement()`, `LowerExpression()`, `LowerVarDecl()`, etc.
     - Variable tracking via `var_ids_` member (replaced `LowerContext`)
     - Public interface unchanged: `LowerToIR()` creates `Lowerer` and calls `Run()`
   - **Benefits**: Better encapsulation, easier to extend, follows C++ best practices

3. **IR instruction set extended with variable support.**
   - **File**: `module/wgx/ir/module.h`
   - **Added instructions**:
     - `InstKind::kVariable` - variable declaration
     - `InstKind::kLoad` - load from variable
     - `InstKind::kStore` - store to variable
   - **Added structures**:
     - `Operand` - instruction operands (ID or constant)
     - `ReturnValueKind::kVariableRef` - return via variable reference
   - **Changes**: `Instruction::result_type` changed from `ValueType` enum to `TypeId`

4. **Lowering extended for variable support.**
   - **File**: `module/wgx/lower/lower_to_ir.cpp`
   - **Added**:
     - `ResolveType()` - converts AST Type to IR TypeId via TypeTable
     - `LowerVarDecl()` - handles `var` statements with initializer
     - `LowerVarInitializer()` - emits store for variable initialization
     - `LowerIdentifierExpression()` - resolves variable references
   - **Current capability**: `var pos: vec4<f32> = vec4<f32>(...); return pos;`

5. **SPIR-V emitter extended for variable support.**
   - **File**: `module/wgx/spirv/emitter.cpp`
   - **Added**:
     - `TypeEmitter` class - emits SPIR-V type instructions from TypeTable
     - `LocalVarInfo` - tracks IR variable -> SPIR-V ID mapping
     - `OpVariable` emission for function-scope variables
     - `OpStore` for variable initialization
     - `OpLoad` for returning variable references
     - `OpCompositeConstruct` for vector construction
   - **Changed**: Uses `module_.type_table` (Module-level) instead of per-Function

6. **Added validation test for local variable return.**
   - **File**: `test/ut/wgx/wgx_spirv_smoke_test.cc`
   - **Test**: `EmitsVertexSpirvBinaryForLocalVariableReturn`
   - **Validates**:
     ```wgsl
     @vertex
     fn main() -> @builtin(position) vec4<f32> {
       var pos: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
       return pos;
     }
     ```
   - **Checks**: `OpVariable`, `OpStore`, `OpLoad`, `OpCompositeConstruct` present

### Validation

- All 13 WGX tests pass
- All 4 generated SPIR-V binaries pass `spirv-val` (Vulkan 1.1)

### Files Modified

```
module/wgx/ir/type.h                      (new)
module/wgx/ir/type.cpp                    (new)
module/wgx/ir/module.h                    (modified)
module/wgx/lower/lower_to_ir.cpp          (modified)
module/wgx/spirv/emitter.cpp              (modified)
module/wgx/CMakeLists.txt                 (modified)
test/ut/wgx/wgx_spirv_smoke_test.cc       (modified)
module/wgx/docs/IR_DEV.md                 (this file)
```

### Current Capability Boundary

1. **Works**:
   - vertex/fragment entry mapping
   - fragment `OriginUpperLeft` execution mode
   - void return path
   - vertex `@builtin(position) vec4<f32>` return with constant vec4 constructor
   - local variable declaration (`var pos: vec4<f32>`) with initializer
   - returning local variable reference (`return pos`)
   - Module-level TypeTable for type management

2. **Not yet supported**:
   - non-constant variable initialization
   - arithmetic operations
   - general expressions, control flow, resources
   - broader IO decoration coverage

---

## 2026-03-25

### Completed

1. SPIR-V emitter no longer returns placeholder text.
   - File: `module/wgx/spirv/emitter.cpp`
   - Status: implemented minimal valid SPIR-V binary generation.
   - Current emitted structure includes:
     - SPIR-V header with version `1.3` (`0x00010300`)
     - `OpCapability Shader`
     - `OpMemoryModel Logical GLSL450`
     - `OpEntryPoint`
     - Fragment-only `OpExecutionMode OriginUpperLeft`
     - `OpName`
     - `OpTypeVoid`, `OpTypeFunction`, `OpFunction`, `OpLabel`, `OpReturn`,
       `OpFunctionEnd`
   - Generator word remains `0` (unknown/unregistered tool id).

2. Lowering robustness update for implicit void return.
   - File: `module/wgx/lower/lower_to_ir.cpp`
   - Status: if function return type is void and no explicit terminal return is
     found, lowering appends an implicit `kReturn` instruction.

3. SPIR-V smoke tests upgraded from placeholder checks to binary checks.
   - File: `test/ut/wgx/wgx_spirv_smoke_test.cc`
   - Status:
     - verifies magic/version/id bound and critical opcodes.
     - verifies fragment `OriginUpperLeft`.
     - verifies current unsupported vertex position-return path fails
       (`WriteToSpirv` returns false).
     - verifies presence of `SpvOpModuleProcessed`.

4. Unit test target now compiles SPIR-V smoke tests under Vulkan build.
   - File: `test/ut/CMakeLists.txt`
   - Status:
     - add `-DWGX_VULKAN=1` when `SKITY_VK_BACKEND` is enabled.
     - link `SPIRV-Headers::SPIRV-Headers` for unit tests when Vulkan backend
       is enabled.

### Validation run on 2026-03-25

1. Command:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.*'`
2. Result:
   - pass (`3/3`).

### Current capability boundary

1. Works:
   - entry point with void return and simple return-only body.
   - vertex/fragment stage mapping and fragment execution mode.
2. Not yet supported:
   - entry point return value emission (for example vertex
     `@builtin(position) vec4<f32>`).
   - general expressions, variables, control flow, resources, and IO decoration
     beyond the current minimal path.

---

## Next Plan

### Phase B: Lowering coverage (AST/Semantic -> IR)

1. Lower return value path and common expressions.
2. Lower local declarations and assignment.
3. Lower if/else and basic loop forms in structured-friendly form.
4. Preserve entry IO metadata from attributes (`@location`, `@builtin`).

Acceptance:
1. Existing WGSL test shaders lower successfully to richer IR.
2. Lowering can represent vertex position output path.

### Phase C: SPIR-V emitter feature expansion

1. Add id allocator and dedup tables:
   - `OpType*` and `OpConstant*` dedup.
2. Add value/type emission:
   - float/vector types
   - constants and composite construction
3. Add interface variable and decoration emission:
   - `OpVariable` for stage inputs/outputs
   - `OpDecorate BuiltIn/Location`
4. Add instruction emission for:
   - arithmetic
   - load/store
   - return with value

Acceptance:
1. Vertex shader returning `@builtin(position) vec4<f32>` emits valid SPIR-V.
2. Generated binaries pass `spirv-val` for supported samples.

### Phase D: Resources and stage semantics

1. Map bind groups to SPIR-V decorations:
   - `DescriptorSet`, `Binding`, storage classes.
2. Add sampled texture/sampler paths for current WGSL subset.
3. Expand execution modes and stage-specific requirements.

Acceptance:
1. Representative resource-binding shaders compile and validate.
2. Reflection metadata remains consistent with existing frontend expectations.

### Phase E: Validation and CI hardening

1. Add `spirv-val` validation step in local script or CI for SPIR-V tests.
2. Add opcode-level golden/smoke tests for each new emitter feature.
3. Keep unsupported features covered by explicit negative tests.

Acceptance:
1. Regressions in emitted SPIR-V structure are caught by tests.
2. CI enforces SPIR-V validity for supported cases.

---

## Immediate next task recommendation

Implement the shortest meaningful feature slice:
1. Vertex entry point with `@builtin(position) -> vec4<f32>`
2. IR return-value representation for vector constant
3. Emitter support for output variable + builtin decoration + return-value path

Rationale:
1. Unblocks real graphics stage output.
2. Forces implementation of key missing type/value/decorate pieces without
   requiring full language coverage first.
