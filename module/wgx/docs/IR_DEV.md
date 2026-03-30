# WGX IR/SPIR-V Status

This document no longer serves as a day-by-day dev log.

Its purpose is to describe the current overall state of the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline, the active
capability boundary of the SPIR-V backend, and the expected starting point for
future development work.

## Read This First

Before continuing SPIR-V backend development, read:

- `module/wgx/docs/IR_REFACTOR_PLAN.md`

That document is now the primary guide for follow-up implementation work.
The current recommendation is to prioritize structural refactoring before adding
more language/backend features.

## Current Direction

The current assessment is:

- the SPIR-V backend is already good enough for validating small vertical slices
- but the IR/lowering/emitter structure still carries several early-stage
  shortcuts
- continuing to add features without cleanup would likely increase coupling
  between lowering and emission, and make later refactors more expensive

Because of that, the near-term focus should be:

1. refactor the IR value/address model
2. reduce backend-side semantic reconstruction
3. make the emitter more type-driven and less `vec4<f32>`-specialized
4. only resume broader feature work after the main P0/P1 refactor items are in
   place

## Current Backend Capability Boundary

The current SPIR-V backend supports a deliberately narrow subset.

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
   - local `var` declaration for supported `vec4<f32>` values
   - local variable initialization with supported `vec4<f32>` constructor values
   - plain local assignment (`=`) for supported values
   - variable-to-variable assignment (`b = a`)
   - return of a local variable

4. **Current arithmetic subset**
   - minimal `vec4<f32>` binary arithmetic
   - supported ops: `add`, `sub`
   - direct return of arithmetic result
   - store of arithmetic result to a local variable before return

5. **Validation workflow already available**
   - WGX SPIR-V smoke tests
   - `spirv-val --target-env vulkan1.1`
   - `spirv-dis` spot-check / disassembly generation
   - local helper script:
     - `./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build`

## Current Structural Limitations

The backend currently still has these important limitations:

1. **IR value modeling is not yet unified**
   - return values, store values, constants, variable refs, and SSA refs are not
     represented in one uniform way

2. **`kLoad` is not yet a real first-class IR flow primitive**
   - variable reads are still partly materialized implicitly in the emitter

3. **Emitter logic is still too specialized around the current `vec4<f32>` path**
   - especially for value materialization and arithmetic emission

4. **Lowering is still too aware of emitter-consumable shapes**
   - this weakens the separation between IR generation and backend translation

5. **Function structure is still effectively single-block / straight-line only**
   - no real control-flow-capable IR structure yet

6. **Validation is strong at the static SPIR-V level, but not yet Vulkan-runtime-integrated**
   - current confidence is based on smoke tests + `spirv-val` + `spirv-dis`

## Practical Rule For Future Agents

If you are continuing work in this area:

1. read `module/wgx/docs/IR_REFACTOR_PLAN.md` first
2. prefer refactoring work over adding new SPIR-V features immediately
3. avoid adding more special-case encoding paths for return/store/materialized
   values unless they clearly align with the refactor plan
4. keep using the current local validation workflow:

```bash
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```

## Validation Baseline

At the time this document was last rewritten, the current supported smoke-test
outputs were verified through:

- `WgxSpirvSmokeTest.*`
- `spirv-val --target-env vulkan1.1`
- `spirv-dis`

This means the currently generated supported `.spv` samples are statically
validated and disassemblable, but future work should still treat the current
backend as a refactor-first in-progress system rather than a feature-complete
compiler backend.
