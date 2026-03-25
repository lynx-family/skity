# WGX IR/SPIR-V Dev Log

This file records incremental development progress for the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline and the next
implementation steps.

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
     - `OpModuleProcessed "skity-wgx-dev"`
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

## Next Plan

### Phase A: Expand IR model (foundation)

1. Split/extend IR types beyond current single-header minimal structs.
2. Add explicit type system:
   - scalar (`bool`, `i32`, `u32`, `f32`)
   - vector/matrix
   - pointer/storage class metadata where needed
3. Add value/instruction coverage:
   - constants, local/global vars, load/store
   - arithmetic and compare
   - call
   - control flow terminators (`branch`, `cond_branch`, `return value`)
4. Add light IR verifier utilities:
   - each block has valid terminator
   - function return consistency
   - type consistency for core instructions

Acceptance:
1. IR can represent current WGSL subset used in tests without loss.
2. Invalid IR patterns fail fast before SPIR-V emission.

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

## Immediate next task recommendation

Implement the shortest meaningful feature slice:
1. Vertex entry point with `@builtin(position) -> vec4<f32>`
2. IR return-value representation for vector constant
3. Emitter support for output variable + builtin decoration + return-value path

Rationale:
1. Unblocks real graphics stage output.
2. Forces implementation of key missing type/value/decorate pieces without
   requiring full language coverage first.
