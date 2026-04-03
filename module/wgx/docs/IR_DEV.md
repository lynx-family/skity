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
6. expand to loops, switch statements, break/continue as the next priority
   before resuming larger feature expansion (texture/sampler, function calls)

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

6. **Structured control flow (if / if-else)**
   - `if` statement with boolean constant condition
   - `if` statement with boolean variable condition
   - `if-else` statement support
   - proper block graph structure (entry, then, else, merge)
   - valid SPIR-V structured control flow (`OpSelectionMerge`, `OpBranchConditional`)

7. **Validation workflow already available**
   - WGX SPIR-V smoke tests
   - `spirv-val --target-env vulkan1.1`
   - `spirv-dis` spot-check / disassembly generation
   - local helper script:
     - `./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build`

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
   - Future: expand to `loop`, `switch`, `break`, `continue`, comparison expressions

6. **Only resume broader feature work after the above cleanup is stable**
   - especially before introducing function calls, texture/sampler ops, or
     matrix/struct heavy features

## Practical Rule For Future Agents

If you are continuing work in this area:

1. read `module/wgx/docs/IR_REFACTOR_PLAN.md` first
2. prefer structural cleanup over immediate feature expansion
3. prioritize multi-block IR / control-flow work before texture-sampler or broader feature expansion
4. avoid reintroducing special-case return/store/materialization encodings that
   bypass `ir::Value`
5. keep using the current local validation workflow:

```bash
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```

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
