# WGX IR/SPIR-V Status

This document no longer serves as a day-by-day dev log.

Its purpose is to describe the current overall state of the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline, the active
capability boundary of the SPIR-V backend, and the expected starting point for
future development work.

## Read This First

Before continuing SPIR-V backend development, read:

- `module/wgx/docs/IR_REFACTOR_PLAN.md`

That document remains useful as the long-lived architecture/reference note for
IR design constraints and refactor priorities.

This document is now the primary place to record the current active slice and
the practical next step for follow-up implementation work.

## Current Active Slice

If you are resuming work from the current state, the recommended next task is:

1. keep the current `texture_2d<f32>` / `sampler` path stable
2. build on the new struct/member-access IR path instead of reintroducing
   flatten-only lowering shortcuts
3. expand struct coverage in the places still outside the validated slice
   before widening more runtime builtin surface

The most recent completed slice is general struct/member access plus
entry-point struct IO for the current validated subset:

- struct types now remain first-class through the current IR path instead of
  being flattened into unrelated local slots during lowering
- IR now has explicit aggregate operations for member-address and member-value
  access
- lowering uses the same address/value model for ordinary member access,
  member assignment, helper-function struct parameters/returns, and
  entry-point struct IO
- SPIR-V emission now lowers those aggregate operations through
  `OpAccessChain` and `OpCompositeExtract`
- entry-point struct input/output flattening now happens at the SPIR-V
  interface boundary rather than inside general lowering
- the validated smoke coverage now includes:
  - vertex `VertexInput` / `VertexOutput` style interface structs
  - helper-function struct parameters
  - struct return followed by member extraction

The current highest-priority missing slices are the broader struct edges that
still sit outside the validated subset:

- nested member access chains such as `a.b.c`
- broader struct initialization / whole-value assignment coverage
- additional entry-point shapes beyond the current scalar/vector member subset
- more validation around fragment-stage struct IO and mixed helper/entry-point
  struct flows

Likely edit targets for this slice:

- `module/wgx/lower/lower_expr.cpp`
- `module/wgx/lower/lower_stmt.cpp`
- `module/wgx/spirv/module_builder.cpp`

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
   - address-producing member access results are represented as
     `Value::PointerSSA(...)`

2. **Explicit address vs value modeling in lowering**
   - expression lowering returns `ExprResult`
   - identifier expressions lower to address/lvalue form
   - `EnsureValue()` inserts explicit `kLoad` when an address is used as a
     value

3. **Explicit load/store/return operands**
   - `kReturn`: zero or one `Value` operand
   - `kLoad`: one address `Value` operand
   - `kStore`: target address `Value` + source value `Value`
   - `kAccess`: one base-address operand + member index
   - `kExtract`: one base-value operand + member index
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

8. **First-class struct/member-access path**
   - struct parameters, locals, and returns are preserved as aggregate types in
     the current IR path
   - member access no longer relies on side maps of per-member variable ids
   - entry-point interface metadata for struct members is carried separately as
     input/output interface variables, while the function body still sees the
     aggregate value/address model

## Current Backend Capability Boundary

The current SPIR-V backend still supports a deliberately narrow subset.

### Supported today

1. **Module / entry-point basics**
   - vertex entry-point emission
   - fragment entry-point emission
   - fragment `OriginUpperLeft` execution mode
   - decorated entry-point input interface emission for the current scalar /
     vector subset
   - basic debug names and builtin position decoration for the supported vertex
     return path

2. **Entry-point input subset**
   - fragment `@location(...)` inputs for the current scalar/vector subset
   - vertex `@location(...)` inputs for the current scalar/vector subset
   - vertex `@builtin(vertex_index)` and `@builtin(instance_index)` input
     plumbing through IR and SPIR-V interface variables

3. **Entry-point struct IO subset**
   - WGSL struct-based entry-point parameters/returns now lower for the current
     scalar/vector member subset
   - repository-style `VertexInput` / `VertexOutput` shaders are part of the
     validated path
   - entry-point struct flattening currently remains a boundary-lowering step,
     not the general representation of structs

4. **Return forms**
   - void return
   - vertex `@builtin(position) vec4<f32>` return
   - entry-point struct return lowered onto multiple output interface variables
   - helper-function struct return plus later member extraction

5. **Current local-value subset**
   - local `var` declaration for scalar and vector values (f32, i32, u32, bool,
     vec2/3/4 of those types)
   - local variable initialization with constructor constants
   - plain local assignment (`=`) for supported values
   - variable-to-variable assignment (`b = a`)
   - return of a local variable
   - member assignment through the aggregate address path for the current
     struct subset

6. **Current arithmetic subset**
   - scalar and vector binary arithmetic for the supported types
   - supported ops: `add`, `sub`
   - direct return of arithmetic result
   - store of arithmetic result to a local variable before return

7. **Global variables**
   - global variable reference, load, and store
   - constant initializers for scalar and vector types
   - WGSL address space mapping (`private`, `uniform`, `storage`, `workgroup`)
   - resource binding attributes (`@group`, `@binding`) with proper SPIR-V decorations
   - automatic struct wrapping for non-struct uniform/storage types (Vulkan compliance)

8. **Structured control flow**
   - `if` statement with boolean constant condition
   - `if` statement with boolean variable condition
   - `if-else` statement support
   - proper block graph structure (entry, then, else, merge)
   - valid SPIR-V structured control flow (`OpSelectionMerge`, `OpBranchConditional`)

9. **Loop support**
   - `loop` with `break`
   - `loop` with `continue`
   - counted loops built from `i32` compare + increment
   - `for` statements lowered onto the existing loop IR
   - `while` statements lowered onto the existing loop IR
   - valid SPIR-V loop structure (`OpLoopMerge`, `OpBranch`, `OpBranchConditional`)

10. **Current comparison subset**
   - scalar comparisons for `f32`, `i32`, `u32`
   - boolean equality / inequality (`==`, `!=`)
   - comparison results can feed `if`, `for`, `while`, and local stores
   - SPIR-V emission uses the corresponding typed compare ops (`OpFOrd*`, `OpIEqual`, `OpSLessThan`, `OpUGreaterThanEqual`, etc.)

11. **Validation workflow already available**
   - WGX SPIR-V smoke tests
   - `spirv-val --target-env vulkan1.1`
   - `spirv-dis` spot-check / disassembly generation
   - local helper script:
     - `./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build`
   - note: the helper currently skips `wgx_vs_main_workgroup.spv` during
     `spirv-val` because workgroup storage is not yet supported for vertex entry points

12. **Function call subset**
   - user-defined function calls with scalar parameters and scalar return values
   - calls from entry points into helper functions defined earlier or later in
     the WGSL module
   - SPIR-V emission of `OpTypeFunction`, `OpFunctionParameter`,
     `OpFunctionCall`, and helper-side `OpReturnValue`
   - dynamic vector construction via `OpCompositeConstruct` for supported
     scalar/vector component types
   - helper-function struct parameters and struct returns for the current
     aggregate/member-access slice

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
   - aggregate support is now present for the current struct/member-access
     slice, but not yet for all WGSL aggregate forms

10. **Struct-heavy entry-point and expression paths are not yet complete**
   - nested struct/member chains still need more coverage and validation
   - whole-struct operations are not yet broadly validated across all forms
   - matrix/struct-heavy expressions should still be treated as an in-progress
     area outside the validated narrow subset

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

If continuing immediately after the current struct/member-access slice, the most
useful next implementation slice is:

1. keep the current `texture_2d<f32>` resource slice stable instead of adding
   more texture kinds immediately
2. expand nested/member-chain and whole-struct behavior on top of the current
   aggregate IR model
3. add more validation for fragment-stage struct IO and helper/entry-point
   aggregate interactions
4. once those edges are stable, return to texture builtin expansion before
   expanding the texture-type matrix

The completed validation baseline for the scalar/vector interface slice is:

- fragment entry point with `@location(0) uv: vec2<f32>`
- `textureSample(texture, sampler, uv)` using that non-constant input
- vertex entry point with `@builtin(vertex_index)` input
- vertex entry point with `@location(0)` input
- validation through `WgxSpirvSmokeTest.*`,
  `./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build`,
  `spirv-val`, and `spirv-dis`

The recommended first validation target for the next slice is:

- nested member access such as `a.b.c`
- whole-struct copy / assignment where the source or destination later feeds
  member access
- fragment entry point using struct input/output on the same aggregate path
- helper-function chains that pass structs across multiple calls

## Clarification: Full Struct Support Direction

The short-term entry-point struct IO work and the long-term struct/member-access
work are related, but they are not the same implementation goal.

If the development goal is only:

- unblock repository shaders that use `VertexInput` / `VertexOutput`
- keep the backend moving with a narrow validation slice

then a temporary flattening-oriented lowering path is acceptable.

If the real development goal is broader:

- support general WGSL struct values in local variables
- support struct parameters and struct returns in ordinary helper functions
- support general member access such as `a.b`, `a.b = x`, `foo(s)`,
  `foo(s.b)`, and later nested access
- support entry-point struct IO as one application of the same aggregate model

then the preferred direction is **not** to keep expanding the temporary
"register one var id per member" lowering pattern.

Instead, treat that pattern only as a narrow compatibility bridge and move the
main design toward a general aggregate/member-access IR model.

## Recommended Design Order For Full Struct Support

If you are continuing this work with the broader goal above, use this order:

1. keep struct types as first-class IR types instead of flattening them away in
   general lowering
2. add explicit aggregate/member-access semantics to IR
   - from a struct address, produce a member address
   - from a struct value, produce a member value
3. make lowering for `MemberAccessor` use that general IR operation instead of
   side maps of per-member local vars
4. make assignment / load / return / call all work on top of the same struct
   value/address model
5. only at the SPIR-V entry-point interface boundary, flatten entry-point
   struct inputs/outputs into interface variables

In other words:

- **general struct/member-access semantics should be the main model**
- **entry-point struct IO flattening should be a boundary-lowering detail**

## What Can Be Reused From The Current Slice

Even if the implementation direction shifts to the broader design above, the
following work remains useful:

- struct type resolution in lowering
- entry-point interface decoration collection from struct members
- verifier/backend allowance for multi-output entry-point returns
- smoke tests for `VertexInput` / `VertexOutput` style shaders

The part that should not become the long-term foundation is:

- representing struct member access by allocating a separate plain local
  variable id for each member and treating member access as a name lookup into
  that flattened side map

That approach is acceptable as a temporary bridge for entry-point compatibility,
but it scales poorly to helper-function struct passing, nested structs, and
general aggregate semantics.

## Change Notes For The Current Struct Slice

To make the current implementation easier to resume, the important code-shape
changes in this slice are:

1. **IR surface changes**
   - `ir::Function` now keeps entry-point interface inputs separately from
     ordinary function parameters
   - interface metadata for flattened entry-point struct members is carried in
     `input_vars` / `output_vars`
   - IR instruction kinds now include aggregate member access operations
     (`kAccess`, `kExtract`)
   - `ir::Value` can now represent address-producing SSA results through
     `PointerSSA`

2. **Lowering changes**
   - ordinary struct parameters, locals, and returns stay as aggregate types in
     the function body
   - `MemberAccessor` lowering now chooses between:
     - member-address access for lvalue paths
     - member-value extraction for rvalue paths
   - assignment/store lowering now accepts general addresses instead of only
     plain local/global variable ids
   - entry-point struct parameters and returns only collect interface metadata
     during lowering; they are not flattened away from the function-body IR

3. **SPIR-V emission changes**
   - aggregate member-address operations lower through `OpAccessChain`
   - aggregate member-value extraction lowers through `OpCompositeExtract`
   - entry-point struct inputs are flattened only when creating SPIR-V `Input`
     interface variables, then written back into the function's aggregate
     parameter storage
   - entry-point struct returns are flattened only when writing to SPIR-V
     `Output` interface variables

4. **Current validation coverage**
   - vertex entry point with struct input/output interface members
   - helper function with struct parameter
   - helper function returning a struct followed by member extraction at the
     call site

5. **Important non-goals of this exact slice**
   - nested member chains are not yet the validated baseline
   - this slice does not claim complete WGSL aggregate support across every
     matrix/array/struct combination
   - the current texture/runtime builtin work should remain stable while the
     remaining struct edges are hardened

## Practical Rule For Future Agents

If you are continuing work in this area:

1. read `module/wgx/docs/IR_REFACTOR_PLAN.md` first
2. prefer structural cleanup over immediate feature expansion
3. if the active goal is full struct/member-access support, prioritize the
   aggregate IR work above before resuming broader builtin expansion
4. otherwise, treat the structured control-flow baseline as complete and move
   on to texture-sampler or broader runtime feature expansion
5. avoid reintroducing special-case return/store/materialization encodings that
   bypass `ir::Value`
6. keep using the current local validation workflow:

```bash
cmake --build out/cmake_host_build -j4
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```
7. remember the language-order mismatch:
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
