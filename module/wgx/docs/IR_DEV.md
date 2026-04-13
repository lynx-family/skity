# WGX IR/SPIR-V Status

This document no longer serves as a day-by-day dev log.

Its purpose is to describe the current overall state of the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline, the active
capability boundary of the SPIR-V backend, and the expected starting point for
future development work.

## Read This First

Before continuing SPIR-V backend development, read:

- `module/wgx/docs/IR_REFACTOR_PLAN.md`

That document remains the primary guide for follow-up implementation work.
The current recommendation is still to prioritize structural refactoring before
adding broader language/backend features.

## Current Direction

The current assessment is:

- the SPIR-V backend is good enough for validating small vertical slices
- the active IR path has now been migrated to a Value-based representation for
  return / load / store / binary operations
- lowering no longer re-encodes those operations through legacy
  `ReturnValueKind` / `Operand` special cases
- the SPIR-V emitter now consumes explicit `Value` operands directly instead of
  reconstructing as much backend-side meaning from bare ids
- a minimal structural verifier now exists, but the IR is still in an early
  architecture-hardening stage
- scalar and vector constants (f32, i32, u32, bool, vec2/3/4 of those types)
  are now supported in both global initializers and function bodies

Because of that, the near-term focus should be:

1. ~~keep tightening the IR structure instead of resuming broad feature work~~ ✓ Core structure is now stable
2. ~~split generic IR verification from backend capability checks more cleanly~~ ✓ Completed
3. ~~make the emitter more type-driven and less `vec4<f32>`-specialized~~ ✓ Completed
4. ~~complete the remaining structural gaps around global storage classes~~ ✓ Completed
5. ~~introduce multi-block IR and structured control flow (if/if-else)~~ ✓ Completed
6. ~~expand to loops, break/continue, comparison expressions, and counted loop building blocks~~ ✓ Completed
7. ~~focus next on the remaining structured-control gaps (`break if`, `switch`)~~
   `break if` and `switch` are now completed
8. ~~resume broader feature work with function calls~~ ✓ First user-function slice completed
   - user-defined function calls now lower through IR `kCall`
   - SPIR-V emission now supports multi-function modules, parameters, and
     `OpFunctionCall`
   - dynamic vector constructors such as `vec4<f32>(x, 0.0, 0.0, 1.0)` now
     lower through IR construction instead of being limited to constant folding
9. texture/sampler builtin calls are now the active broader runtime feature area
   - the first resource-handle vertical slice is now in place for
     `textureDimensions(texture_2d<f32>)`
   - resource-handle lowering now recognizes `texture_2d<f32>` and `sampler`
   - IR now has a dedicated builtin-call path instead of forcing texture
     intrinsics through the user-function `kCall` instruction
   - SPIR-V emission now supports the minimum image-query path needed for
     `OpTypeImage` + `OpImageQuerySizeLod`
   - the minimum sampled-image path is now also in place for fragment
     `textureSample(texture, sampler, coord)` with `texture_2d<f32>`
   - explicit-lod and load-style texture builtins are now also wired up for the
     same `texture_2d<f32>` resource slice
   - current texture work is intentionally staying on the existing
     `texture_2d<f32>` path instead of expanding to additional texture kinds yet

## Current IR State

The active function-body IR now has these properties:

1. **Unified value flow through `ir::Value`**
   - constants are represented as `Value`
   - SSA results are represented as `Value::SSA(...)`
   - local variable references are represented as `Value::Variable(...)`

2. **Explicit address vs value modeling in lowering**
   - expression lowering returns `ExprResult`
   - identifier expressions lower to address/lvalue form
   - `EnsureValue()` inserts explicit `kLoad` when an address is used as a
     value

3. **Explicit load/store/return operands**
   - `kReturn`: zero or one `Value` operand
   - `kLoad`: one variable `Value` operand
   - `kStore`: target variable `Value` + source value `Value`
   - `kBinary`: lhs `Value` + rhs `Value`
   - `kConstruct`: N value operands for vector construction
   - `kCall`: N value operands for user-function call arguments

4. **Separated variable id vs SSA id allocation**
   - `var_id` is allocated independently from SSA ids
   - `result_id` is now used for SSA-producing instructions only
   - `kVariable` declarations carry their own `var_id`

5. **Initial structural verification exists**
   - instruction operand shapes are checked before backend support checks run
   - verifier coverage is still intentionally minimal and should be expanded

6. **Global variable metadata**
   - `ir::Module` carries a `global_variables` map (variable id -> `GlobalVariable`)
   - `GlobalVariable` holds `type`, `storage_class`, `initializer`, `group`, `binding`
   - lowering extracts address space, binding attributes, and constant initializers
   - SPIR-V emitter uses storage class and emits `OpVariable` with initializers and decorations

7. **Multi-function module lowering**
   - lowering no longer stops at the entry point; user-defined callees are
     recursively lowered into the same `ir::Module`
   - `ir::Function` now carries parameter metadata used by SPIR-V emission
   - current lowering order ensures WGSL source order does not need to match
     SPIR-V function emission order
   - this matters because WGSL allows calling a function defined later in the
     module, while SPIR-V emission still needs callee definitions/types ready
     before `OpFunctionCall` use sites

## Current Backend Capability Boundary

The current SPIR-V backend still supports a deliberately narrow subset.

### Supported today

1. **Module / entry-point basics**
   - vertex entry-point emission
   - fragment entry-point emission
   - fragment `OriginUpperLeft` execution mode
   - basic debug names and builtin position decoration for the supported vertex
     return path

2. **Return forms**
   - void return
   - vertex `@builtin(position) vec4<f32>` return

3. **Current local-value subset**
   - local `var` declaration for scalar and vector values (f32, i32, u32, bool,
     vec2/3/4 of those types)
   - local variable initialization with constructor constants
   - plain local assignment (`=`) for supported values
   - variable-to-variable assignment (`b = a`)
   - return of a local variable

4. **Current arithmetic subset**
   - scalar and vector binary arithmetic for the supported types
   - supported ops: `add`, `sub`
   - direct return of arithmetic result
   - store of arithmetic result to a local variable before return

5. **Global variables**
   - global variable reference, load, and store
   - constant initializers for scalar and vector types
   - WGSL address space mapping (`private`, `uniform`, `storage`, `workgroup`)
   - resource binding attributes (`@group`, `@binding`) with proper SPIR-V decorations
   - automatic struct wrapping for non-struct uniform/storage types (Vulkan compliance)

6. **Structured control flow**
   - `if` statement with boolean constant condition
   - `if` statement with boolean variable condition
   - `if-else` statement support
   - proper block graph structure (entry, then, else, merge)
   - valid SPIR-V structured control flow (`OpSelectionMerge`, `OpBranchConditional`)

7. **Loop support**
   - `loop` with `break`
   - `loop` with `continue`
   - counted loops built from `i32` compare + increment
   - `for` statements lowered onto the existing loop IR
   - `while` statements lowered onto the existing loop IR
   - valid SPIR-V loop structure (`OpLoopMerge`, `OpBranch`, `OpBranchConditional`)

8. **Current comparison subset**
   - scalar comparisons for `f32`, `i32`, `u32`
   - boolean equality / inequality (`==`, `!=`)
   - comparison results can feed `if`, `for`, `while`, and local stores
   - SPIR-V emission uses the corresponding typed compare ops (`OpFOrd*`, `OpIEqual`, `OpSLessThan`, `OpUGreaterThanEqual`, etc.)

9. **Validation workflow already available**
   - WGX SPIR-V smoke tests
   - `spirv-val --target-env vulkan1.1`
   - `spirv-dis` spot-check / disassembly generation
   - local helper script:
     - `./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build`
   - note: the helper currently skips `wgx_vs_main_workgroup.spv` during
     `spirv-val` because workgroup storage is not yet supported for vertex entry points

10. **Function call subset**
   - user-defined function calls with scalar parameters and scalar return values
   - calls from entry points into helper functions defined earlier or later in
     the WGSL module
   - SPIR-V emission of `OpTypeFunction`, `OpFunctionParameter`,
     `OpFunctionCall`, and helper-side `OpReturnValue`
   - dynamic vector construction via `OpCompositeConstruct` for supported
     scalar/vector component types

## Current Structural Limitations

The backend currently still has these important limitations:

1. **Instruction storage is still a shared struct**
   - `ir::Instruction` still contains fields that are only meaningful for some
     instruction kinds
   - this is better than the old encoding, but still not the final ideal shape

2. **The verifier is functional but still evolving**
   - `ir::Verifier` provides structural validation separate from backend checks
   - checks operand/result shapes, use-def chains, and basic type validity
   - dominance checking and richer type verification are future enhancements

3. ~~**Emitter logic is still too specialized around the current `vec4<f32>` path**~~ ✓ Completed
   - `MaterializeConstVector` now supports arbitrary vector dimensions (vec2, vec3, vec4)
   - removed hardcoded `vec4_type_id_` and `entry_vec4_type_` special cases
   - SPIR-V type ids are now derived from each instruction's `result_type` dynamically

4. ~~**Global variable storage class is not yet mapped from WGSL address spaces**~~ ✓ Completed
   - `ir::Module::GlobalVariable` struct carries `storage_class`, `group`, `binding`
   - lowering extracts address space from WGSL `var<...>` and maps to `ir::StorageClass`
   - lowering extracts `@group` and `@binding` attributes
   - SPIR-V emitter uses correct `SpvStorageClass` and emits resource decorations

5. ~~**Function structure is still effectively single-block / straight-line only**~~ ✓ Completed
   - `Function` now has a `blocks` vector and `entry_block_id` for multi-block IR
   - `Block` has `id` and `name` for identification
   - terminator instructions (`kBranch`, `kCondBranch`, `kReturn`) end each block

6. **Validation is strong at the static SPIR-V level, but not yet Vulkan-runtime-integrated**
   - current confidence is based on smoke tests + `spirv-val` + `spirv-dis`

7. **Structured control flow is still a subset, not full WGSL control flow**
   - `break if` in loop `continuing` blocks is now lowered through conditional
     exit from the continue block to loop merge vs loop header
   - `switch` is now lowered through nested equality tests and structured case
     blocks with shared exit merge
   - loop support is intentionally focused on the current structured subset

8. **Comparison/arithmetic support is still intentionally incomplete**
   - compare support is currently scalar-focused
   - the newly supported counted-loop path relies on scalar integer arithmetic
   - broader operator coverage (more scalar/vector ops, casts, richer expressions) remains future work

9. **Function call coverage is still intentionally narrow**
   - the current call path is for user-defined WGSL functions, not builtin
     texture/sampler/runtime intrinsics
   - return/value handling is validated for the current scalar and vector
     subsets, not full WGSL aggregate semantics

## Next Planned Refactor Steps

The current recommendation for the next steps is:

1. ~~**Move IR verification into a dedicated module/pass**~~ ✓ Completed
   - `ir::Verifier` class in `module/wgx/ir/verifier.h`
   - structural validity checks are now independent from backend support checks
   - `ir::Verify()` convenience functions for one-shot verification
   - emitter now delegates to verifier instead of inline checks

2. ~~**Decouple lowering from emitter-specific shapes**~~ ✓ Completed
   - lowering no longer checks backend capabilities (e.g., "is this type supported?")
   - lowering generates IR for any valid WGSL type (f32, i32, vec2/3/4, etc.)
   - emitter's `SupportsCurrentIR()` rejects unsupported types/shapes
   - variable types are now tracked per-variable instead of assuming vec4<f32>

3. ~~**Make emission more type-driven**~~ ✓ Completed
   - SPIR-V type ids are derived from each instruction/value type dynamically
   - constant materialization supports vec2/vec3/vec4 through `EmitConstantComposite`
   - emitter no longer assumes all values are position-style `vec4<f32>`

4. ~~**Map WGSL address spaces to IR/SPIR-V storage classes**~~ ✓ Completed
   - `uniform` / `storage` / `workgroup` / `private` are propagated through lowering
   - SPIR-V emission now uses the correct `SpvStorageClass` on `OpVariable`
   - resource globals now emit DescriptorSet / Binding decorations
   - non-struct uniform/storage values are wrapped for Vulkan-compatible Block layout

5. ~~**Introduce multi-block IR and structured control flow**~~ ✓ Completed
   - `Function` now uses `blocks` vector with `entry_block_id` instead of single `entry_block`
   - `Block` has `id` and `name` fields for identification
   - New instruction kinds: `kBranch` (unconditional) and `kCondBranch` (conditional)
   - `Instruction` has `target_block`, `true_block`, `false_block`, `merge_block` fields
   - `IsTerminator()` method identifies block-ending instructions
   - Lowering creates proper block graph with entry, then, else, and merge blocks
   - SPIR-V emitter generates `OpSelectionMerge`, `OpBranchConditional`, `OpBranch`
   - Basic if/if-else supported with boolean constants and boolean variables as conditions
   - Later work expanded this to `loop`, `for`, `while`, `break`, `continue`, and scalar comparison expressions

6. ~~**Add loop/control-flow expansion on top of the multi-block IR**~~ ✓ Completed for the current subset
   - `loop` lowers with explicit header/body/continue/merge blocks
   - `for` and `while` lower onto the same structured loop representation
   - loop-condition blocks are kept separate from loop headers so emitted SPIR-V satisfies structured-control constraints
   - smoke tests + `spirv-val` now cover the supported `loop` / `for` / `while` subset

7. ~~**Reintroduce user-function calls on top of the current IR**~~ ✓ Completed for the current subset
   - lowering recursively materializes helper functions into the IR module
   - SPIR-V emission now handles helper function definitions, parameters, and
     `OpFunctionCall`
   - smoke tests cover helper calls and dynamic vector constructors that depend
     on runtime values

8. **Only resume broader runtime feature work after the current call path is stable**
   - especially for texture/sampler builtins, casts, and matrix/struct-heavy expressions

## Recommended Runtime Feature Expansion Order

The next runtime feature work should start with texture/sampler builtins, but it
should be staged instead of attempting the full surface area in one step.

1. **Add IR type support for resource handles**
   - extend lowering so `texture_2d<f32>` and `sampler` no longer fail type resolution
   - do not model all handles as an undifferentiated storage-class-only concept;
     the IR must retain enough type information to distinguish image vs sampler
     and to recover sampled element type during SPIR-V emission
   - teach the SPIR-V type emitter to generate the corresponding handle types
     (`OpTypeImage`, `OpTypeSampler`) instead of only pointer/scalar/vector forms

2. **Introduce explicit IR support for builtin runtime intrinsics**
   - do not overload the current user-function `kCall` path for texture/sampler
     builtins
   - add a dedicated builtin/intrinsic representation in IR so lowering and the
     emitter can distinguish user calls from operations such as
     `textureDimensions()` and `textureSample()`
   - keep builtin lowering on top of the same `ir::Value` discipline already used
     for load/store/binary/construct/call

3. ~~**Implement `textureDimensions(texture)` first as the minimum vertical slice**~~ ✓ Completed
   - handle typing, builtin lowering, and SPIR-V image query emission are now
     wired up for the `texture_2d<f32>` path
   - smoke tests, `spirv-val`, and `spirv-dis` now cover
     `wgx_vs_main_texture_dimensions.spv`

4. ~~**Implement `textureSample(texture, sampler, coord)` second**~~ ✓ First slice completed
   - sampled-image emission (`OpTypeSampledImage`, `OpSampledImage`) and the
     first sampling instruction path (`OpImageSampleImplicitLod`) are now wired
     up for fragment shaders
   - the current supported sampling slice is intentionally narrow:
     `texture_2d<f32>` + `sampler` + `vec2<f32>` coord + fragment-stage
     implicit-lod sampling
   - reuse of the already working GLSL/MSL texture builtin behavior remains the
     semantic reference for further expansion

5. ~~**Add explicit-lod and load-style texture builtins on top of the first sampling slice**~~ ✓ Completed
   - `textureSampleLevel(texture, sampler, coord, level)` now lowers and emits
     through `OpSampledImage` + `OpImageSampleExplicitLod`
   - `textureLoad(texture, coords, level)` now lowers and emits through
     `OpImageFetch`
   - smoke tests, `spirv-val`, and `spirv-dis` now cover
     `wgx_fs_main_texture_sample_level.spv` and
     `wgx_fs_main_texture_load.spv`

6. **Only then expand to the broader texture builtin surface**
   - examples: `textureNumLevels`, `textureNumLayers`, comparison samplers, and
     array/cube/3d texture forms
   - current recommendation is to defer texture-type expansion for now and keep
     the implementation focused on the already validated `texture_2d<f32>` path

## Concrete Next Slice

If continuing immediately after the current user-function-call work, the most
useful next implementation slice is:

1. keep the current `texture_2d<f32>` resource slice stable instead of adding
   more texture kinds immediately
2. add entry-point input/output interface lowering as needed for non-constant
   fragment sample coordinates
3. if texture work resumes later, extend the builtin surface before extending
   the texture-type matrix
4. defer array/cube/3d/comparison texture expansion until there is a concrete
   product need

## Practical Rule For Future Agents

If you are continuing work in this area:

1. read `module/wgx/docs/IR_REFACTOR_PLAN.md` first
2. prefer structural cleanup over immediate feature expansion
3. treat the structured control-flow baseline as complete and move on to
   texture-sampler or broader runtime feature expansion
4. avoid reintroducing special-case return/store/materialization encodings that
   bypass `ir::Value`
5. keep using the current local validation workflow:

```bash
cmake --build out/cmake_host_build -j4
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```
6. remember the language-order mismatch:
   - WGSL helper functions may be called before their source declaration
   - SPIR-V still needs function types/definitions available in emission order
   - keep lowering/emission responsible for that ordering instead of imposing a
     WGSL source-order restriction

## Validation Baseline

At the time this document was last rewritten, the actively supported smoke-test
outputs were verified through:

- `WgxSpirvSmokeTest.*`
- `spirv-val --target-env vulkan1.1`
- `spirv-dis`

This means the currently generated supported `.spv` samples are statically
validated and disassemblable, but future work should still treat the current
backend as a refactor-first in-progress system rather than a feature-complete
compiler backend.
