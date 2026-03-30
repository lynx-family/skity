# WGX IR/SPIR-V Dev Log

This file records incremental development progress for the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline and the next
implementation steps.

## Recommended Local Validation Workflow

For WGX SPIR-V work, use the following local verification order:

1. Rebuild and run the smallest relevant smoke tests first.
2. Validate dumped `.spv` outputs with `spirv-val --target-env vulkan1.1`.
3. Spot-check the generated disassembly with `spirv-dis` when adding a new
   emitter/lowering slice or when debugging validator failures.

Suggested commands:

```bash
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```

Equivalent manual steps:

```bash
./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.*'
for f in out/cmake_host_build/*.spv; do
  /usr/local/bin/spirv-val --target-env vulkan1.1 "$f"
done
for f in out/cmake_host_build/*.spv; do
  /usr/local/bin/spirv-dis "$f" -o "${f%.spv}.spvasm"
done
```

Notes:
- `spirv-val` is the required legality check.
- `spirv-dis` is the recommended structure/inspection check; it is especially
  useful for verifying opcode shape, id use order, and decorations.


## 2026-03-30

### Summary

Completed the first arithmetic slice after assignment stabilization: the IR and
SPIR-V pipeline now supports minimal `vec4<f32>` binary add/sub expressions for
currently representable local values, and variable-to-variable assignment is now
covered by smoke tests.

### Completed

1. **Locked in variable-copy assignment coverage.**
   - **File**: `test/ut/wgx/wgx_spirv_smoke_test.cc`
   - **Added test**: `EmitsVertexSpirvBinaryForVariableCopyAssignmentReturn`
   - **Validates**:
     ```wgsl
     @vertex
     fn vs_main() -> @builtin(position) vec4<f32> {
       var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
       var b: vec4<f32>;
       b = a;
       return b;
     }
     ```
   - **Effect**:
     - confirms the existing `{dst_var_id, src_var_id}` store path lowers and
       emits as expected

2. **Extended IR to model minimal binary arithmetic results.**
   - **File**: `module/wgx/ir/module.h`
   - **Added**:
     - `InstKind::kBinary`
     - `BinaryOpKind::{kAdd, kSubtract}`
     - `ReturnValueKind::kValueRef`
     - function-local SSA-style value id allocation for arithmetic results
   - **Effect**:
     - arithmetic can now produce explicit IR values without forcing an
       immediate store to a local variable

3. **Lowering now supports narrow `vec4<f32>` add/sub expressions.**
   - **File**: `module/wgx/lower/lower_to_ir.cpp`
   - **Added/changed**:
     - `LowerBinaryExpression()`
     - binary operand collection through existing expression lowering
     - `LowerStoreValue()` and return lowering updated to accept arithmetic
       value results
   - **Current supported subset**:
     - `+` and `-` only
     - operands must lower to supported `vec4<f32>` values
     - currently practical operand shapes are local variables and previously
       produced arithmetic values
   - **Still excluded**:
     - multiply/divide/modulo
     - scalar-vector mixing
     - member/index/access-chain arithmetic operands
     - compound assignment (`+=`, `-=`)

4. **SPIR-V emitter now emits minimal vector arithmetic.**
   - **File**: `module/wgx/spirv/emitter.cpp`
   - **Added/changed**:
     - arithmetic value tracking for IR `kBinary` results
     - `EmitBinary()` for:
       - `OpFAdd`
       - `OpFSub`
     - unified `MaterializeValue*()` helpers now handle return/store operand
       materialization consistently
     - binary result ids are now allocated at actual emission time instead of
       being pre-reserved during analysis
   - **Effect**:
     - `return a + b;` and `c = a - b; return c;` now emit valid SPIR-V in the
       current subset
     - generated arithmetic SPIR-V now also passes `spirv-val` validation

5. **Added smoke coverage for arithmetic return/store paths.**
   - **File**: `test/ut/wgx/wgx_spirv_smoke_test.cc`
   - **Added tests**:
     - `EmitsVertexSpirvBinaryForVectorAddReturn`
     - `EmitsVertexSpirvBinaryForVectorSubAssignmentReturn`
   - **Checks**:
     - `OpFAdd` / `OpFSub`
     - `OpLoad`
     - `OpStore`
     - builtin position decoration

### Validation

1. Rebuilt unit test target:
   - `cmake --build out/cmake_host_build --target skity_unit_test`
2. Ran targeted variable-copy assignment test:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.EmitsVertexSpirvBinaryForVariableCopyAssignmentReturn'`
   - result: pass (`1/1`)
3. Ran targeted arithmetic tests:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.EmitsVertexSpirvBinaryForVectorAddReturn'`
   - result: pass (`1/1`)
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.EmitsVertexSpirvBinaryForVectorSubAssignmentReturn'`
   - result: pass (`1/1`)
4. Ran WGX SPIR-V smoke suite:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.*'`
   - result: pass (`8/8`)
5. Ran `spirv-val` on all generated smoke-test SPIR-V binaries:
   - command: `for f in out/cmake_host_build/*.spv; do /usr/local/bin/spirv-val --target-env vulkan1.1 "$f"; done`
   - result: pass (all generated `.spv` files validated)
6. Spot-checked disassembly for generated smoke-test SPIR-V binaries with `spirv-dis`:
   - recommended script: `./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build`
   - manual disassembly command: `for f in out/cmake_host_build/*.spv; do /usr/local/bin/spirv-dis "$f" -o "${f%.spv}.spvasm"; done`
   - purpose: inspect emitted instructions, ids, entry-point/interface decorations, and arithmetic/load/store structure alongside `spirv-val`

### Current Capability Boundary

1. **Works**:
   - vertex/fragment entry mapping
   - fragment `OriginUpperLeft` execution mode
   - void return path
   - vertex `@builtin(position) vec4<f32>` return with constant vec4 constructor
   - local variable declaration with or without initializer for `vec4<f32>`
   - local variable assignment using `=` for supported `vec4<f32>` values
   - variable-to-variable assignment (`b = a`)
   - `vec4<f32>` binary add/sub for supported local values
   - returning local variable reference (`return pos`)
   - returning direct arithmetic value (`return a + b`)
   - module-level `TypeTable` for type management

2. **Not yet supported**:
   - compound assignment (`+=`, `-=`, ...)
   - non-identifier assignment lhs (member/index/access chain)
   - multiply/divide/modulo and broader arithmetic
   - constants used directly as binary operands without prior materialization
   - general expressions, control flow, resources
   - broader IO decoration coverage

### Next Development Plan

1. Extend binary operand materialization beyond identifiers/SSA values so
   constant vec constructors and other already-supported expression forms can
   participate directly in arithmetic.
2. Broaden arithmetic coverage incrementally from `add/sub` to `mul/div`, while
   keeping SPIR-V validation (`spirv-val`) plus disassembly spot checks
   (`spirv-dis`) in the default smoke-test workflow for each new slice.
3. Start shaping a more general lvalue/address path for assignment targets
   before adding member/index writes or compound assignment.

---

## 2026-03-27

### Summary

Completed the next Phase B slice for local variable mutation: assignment
statements now lower to IR and emit valid SPIR-V for the currently supported
`vec4<f32>` vertex-position path.

### Completed

1. **Lowering extended to support simple assignment statements.**
   - **File**: `module/wgx/lower/lower_to_ir.cpp`
   - **Added**:
     - `LowerAssignStatement()` for `identifier = expression`
     - `LowerStoreValue()` helper shared by variable initialization and later
       assignment
   - **Current supported subset**:
     - plain assignment only (`=`)
     - lhs must be a local identifier
     - rhs may be a supported `vec4<f32>` constant or local variable reference
   - **Still excluded**:
     - compound assignment (`+=`, `-=`, ...)
     - member/index/access-chain lhs forms

2. **IR store path generalized for assignment reuse.**
   - **Files**: `module/wgx/lower/lower_to_ir.cpp`,
     `module/wgx/ir/module.h`
   - **Design update**:
     - `kStore` continues to represent writes to variables
     - store operands may now carry either:
       - destination variable id + four `f32` constants, or
       - destination variable id + source variable id
   - **Effect**:
     - variable initialization and assignment now share the same IR write form
     - prepares the pipeline for future variable-to-variable copies and richer
       rhs expression support

3. **SPIR-V emitter extended for assignment-driven stores.**
   - **File**: `module/wgx/spirv/emitter.cpp`
   - **Added/changed**:
     - `EmitStore()` now accepts two store shapes:
       - constant vector write: `OpCompositeConstruct` + `OpStore`
       - variable copy write: `OpLoad` + `OpStore`
   - **Current effect**:
     - WGSL local assignment updates can reuse the existing local variable
       storage path without adding new IR instructions

4. **Added smoke coverage for assigned local variable return.**
   - **File**: `test/ut/wgx/wgx_spirv_smoke_test.cc`
   - **Test**: `EmitsVertexSpirvBinaryForAssignedLocalVariableReturn`
   - **Validates**:
     ```wgsl
     @vertex
     fn main() -> @builtin(position) vec4<f32> {
       var pos: vec4<f32>;
       pos = vec4<f32>(0.0, 0.0, 0.0, 1.0);
       return pos;
     }
     ```
   - **Checks**: `OpVariable`, `OpStore`, `OpLoad`,
     `OpCompositeConstruct`, builtin position decoration

### Validation

1. Rebuilt unit test target:
   - `cmake --build out/cmake_host_build --target skity_unit_test`
2. Ran targeted new smoke test:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.EmitsVertexSpirvBinaryForAssignedLocalVariableReturn'`
   - result: pass (`1/1`)
3. Ran WGX SPIR-V smoke suite:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.*'`
   - result: pass (`5/5`)

### Current Capability Boundary

1. **Works**:
   - vertex/fragment entry mapping
   - fragment `OriginUpperLeft` execution mode
   - void return path
   - vertex `@builtin(position) vec4<f32>` return with constant vec4 constructor
   - local variable declaration with or without initializer for `vec4<f32>`
   - local variable assignment using `=` for supported `vec4<f32>` values
   - returning local variable reference (`return pos`)
   - module-level `TypeTable` for type management

2. **Not yet supported**:
   - compound assignment (`+=`, `-=`, ...)
   - non-identifier assignment lhs (member/index/access chain)
   - arithmetic operations
   - general expressions, control flow, resources
   - broader IO decoration coverage

---

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
2. Lower local declarations and assignment. (done for simple local `=` path)
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

Implement the next shortest meaningful feature slice:
1. Add explicit coverage for variable-to-variable assignment
   - example: `var a = vec4<f32>(...); var b: vec4<f32>; b = a; return b;`
   - status: emitter/lowering path appears structurally ready; lock it in with
     a smoke test before expanding expression support
2. Extend lowering/emitter support for simple arithmetic on supported vector values
   - start with `vec4<f32>` add/sub where both operands are already representable
3. Keep assignment support intentionally narrow until arithmetic/value modeling is stable

Rationale:
1. Variable-copy assignment is already structurally supported by the current store path
   and should be locked in with tests before broader expression work.
2. Simple arithmetic is the next feature that materially expands shader usefulness
   without requiring control flow or resource support first.

---

## Actionable TODO (refined on 2026-03-30)

### Current code reality check

1. **Variable-to-variable assignment is likely already implemented end-to-end.**
   - `LowerAssignStatement()` already lowers plain `identifier = expression`
   - `LowerExpression()` already accepts identifier rhs and returns a variable ref
   - `LowerStoreValue()` already emits a store shape `{dst_var_id, src_var_id}`
   - `EmitStore()` already translates that store shape into `OpLoad + OpStore`
2. **The main missing piece is coverage, not core plumbing.**
   - there is currently a smoke test for assigning a constant vector to a local,
     but no dedicated smoke test that proves `b = a` works
3. **Arithmetic is the next real implementation gap.**
   - parser and AST already expose `BinaryExp` and additive operators
   - lowering currently only accepts:
     - vec4<f32> constructor constants
     - identifier expressions
   - emitter currently only accepts:
     - variable declarations
     - stores
     - returns

### Recommended next implementation order

1. **Variable-copy assignment coverage is done. Keep it as a regression check.**
   - Add a smoke test for:
     ```wgsl
     @vertex
     fn vs_main() -> @builtin(position) vec4<f32> {
       var a: vec4<f32> = vec4<f32>(0.0, 0.0, 0.0, 1.0);
       var b: vec4<f32>;
       b = a;
       return b;
     }
     ```
   - Validate presence of:
     - `OpVariable`
     - `OpStore`
     - `OpLoad`
     - builtin position decoration
   - Prefer also checking that more than one store/load appears if test helpers
     are extended later

2. **Minimal IR for binary vector arithmetic is done for add/sub. Extend carefully.**
   - Add an IR instruction for binary arithmetic on supported value kinds
   - Keep the first slice intentionally narrow:
     - `vec4<f32> + vec4<f32>`
     - `vec4<f32> - vec4<f32>`
     - operands limited to currently representable values (constants or locals)

3. **Lowering support for narrow `BinaryExp` add/sub is done. Next expand incrementally.**
   - Add `LowerBinaryExpression()` in `module/wgx/lower/lower_to_ir.cpp`
   - Accept only `ast::BinaryOp::kAdd` and `ast::BinaryOp::kSubtract`
   - Reject all other ops for now
   - Reuse current `vec4<f32>` type checks so unsupported shapes fail clearly

4. **SPIR-V emission for minimal arithmetic results is done. Next expand instruction coverage.**
   - Map the new IR arithmetic instruction to:
     - `OpFAdd`
     - `OpFSub`
   - Ensure arithmetic results can feed:
     - `kStore`
     - return-with-value path
   - Decide whether to model arithmetic as SSA values directly before adding
     more expression forms

5. **Only after arithmetic is stable, widen assignment/value modeling.**
   - compound assignment (`+=`, `-=`)
   - more lhs forms (member/index/access chain)
   - richer mixed expression trees

### Suggested file-level entry points

1. `test/ut/wgx/wgx_spirv_smoke_test.cc`
   - add the variable-copy assignment smoke test first
2. `module/wgx/ir/module.h`
   - add the minimal arithmetic IR shape if proceeding to binary expressions
3. `module/wgx/lower/lower_to_ir.cpp`
   - add binary expression lowering after the test-only assignment slice
4. `module/wgx/spirv/emitter.cpp`
   - add arithmetic instruction emission once the IR shape is settled

### Validation plan for the next slice

1. Build unit tests:
   - `cmake --build out/cmake_host_build --target skity_unit_test`
2. Run the new targeted smoke test first:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.EmitsVertexSpirvBinaryForVariableCopyAssignmentReturn'`
3. Run the full WGX SPIR-V smoke suite:
   - `./out/cmake_host_build/test/ut/skity_unit_test --gtest_filter='WgxSpirvSmokeTest.*'`
4. If arithmetic lands, add a matching targeted arithmetic smoke test before rerunning the suite
