# WGX IR/SPIR-V Status

This document records the current practical status of the
`WGSL -> AST -> Semantic Resolve -> IR -> SPIR-V` pipeline in `module/wgx`.

It is not a historical dev log. Its job is to answer four questions:

1. what is implemented today
2. what is actually validated today
3. what still blocks repository-style rendering shaders
4. what the next implementation order should be

## Read This First

Before changing the SPIR-V backend, also read:

- `module/wgx/docs/IR_REFACTOR_PLAN.md`

That document remains the long-lived architecture note.
This document is the short practical status and next-work guide.

## Current Pipeline Status

The SPIR-V path is real and in active use for validation:

1. parse WGSL into AST
2. resolve symbols/semantic bindings
3. lower into IR
4. emit SPIR-V from IR

`Program::WriteToSpirv()` already goes through the IR path rather than a
backend-local AST printer.

## Current IR State

The active IR path has these important properties:

1. `ir::Value` is the common value model for constants, SSA results, local/global
   variables, and address-producing member-access results
2. lowering distinguishes address vs value explicitly through `ExprResult`
3. `kLoad`, `kStore`, `kReturn`, `kBinary`, `kConstruct`, `kCall`, and
   `kBuiltinCall` all use explicit typed `Value` operands
4. the IR now supports multi-block control flow with `kBranch` and
   `kCondBranch`
5. user-defined helper functions lower into the same `ir::Module`
6. struct/member-access is now first-class in the current validated slice
7. global variable metadata includes storage class, binding, and initializer

The current IR structure is good enough to continue broader runtime feature
work. The main remaining limitations are backend capability breadth and test
coverage, not the old single-block IR shape.

## Current SPIR-V Capability Boundary

The current backend supports a deliberately narrow but real graphics subset.

### Supported today

1. vertex and fragment entry points
2. fragment `OriginUpperLeft`
3. entry-point inputs and outputs for the current scalar/vector subset
4. vertex builtins:
   - `@builtin(position)` output
   - `@builtin(vertex_index)` input
   - `@builtin(instance_index)` input
5. struct-based entry-point IO for the current scalar/vector member subset
6. local variables and plain assignment for:
   - `f32`
   - `i32`
   - `u32`
   - `bool`
   - `vec2/3/4` of those scalar types
7. global variables with WGSL address-space mapping:
   - `private`
   - `uniform`
   - `storage`
   - `workgroup`
8. resource bindings with `@group` / `@binding`
9. structured control flow:
   - `if`
   - `if/else`
   - `loop`
   - `for`
   - `while`
   - `switch`
   - `break`
   - `continue`
   - `break if`
10. user-defined helper-function calls for the current subset
11. struct/member-access for the current validated aggregate slice
12. texture/sampler builtins for the current `texture_2d<f32>` slice:
   - `textureDimensions`
   - `textureSample`
   - `textureSampleLevel`
   - `textureLoad`

### Current arithmetic / expression subset

The backend now supports a meaningful first rendering-oriented expression slice:

1. binary arithmetic:
   - `add`
   - `sub`
   - `mul`
   - `div`
2. scalar comparisons:
   - `==`
   - `!=`
   - `<`
   - `>`
   - `<=`
   - `>=`
3. explicit scalar casts:
   - `f32(...)`
   - `i32(...)`
   - `u32(...)`
4. dynamic vector constructors for supported scalar/vector types
5. matrix constructors for:
   - `mat2x2<f32>`
   - `mat3x3<f32>`
   - `mat4x4<f32>`
   from either scalar arguments or column vectors
6. matrix multiply for:
   - `mat2x2<f32> * vec2<f32>`
   - `mat3x3<f32> * vec3<f32>`
   - `mat4x4<f32> * vec4<f32>`
   - `mat4x4<f32> * mat4x4<f32>`
7. math builtins for the current validated float slice:
   - `dot`
   - `distance`
   - `sqrt`
   - `abs`
   - `sign`
   - `max`
   - `clamp`
   - `mix`
   - `select`
   - `atan`
   - `length`
   - `normalize`
   - `inverseSqrt` / `inversesqrt`

The most important remaining limitation is no longer basic arithmetic.
It is now the remaining math builtin surface and a few expression-shape gaps
used by repository-style shaders.

## What Is Actually Validated Today

The current baseline is not just static SPIR-V emission. It includes:

1. unit smoke coverage through `WgxSpirvSmokeTest.*`
2. `spirv-val --target-env vulkan1.1`
3. `spirv-dis`
4. Vulkan runtime pipeline creation through SwiftShader

The current local validation workflow is:

```bash
python3 tools/test-runner.py --suite=unit --build-dir out/cmake_host_build --filter="WgxSpirvSmokeTest.*"
python3 tools/test-runner.py --suite=unit --build-dir out/cmake_host_build --filter="WgxVulkanPipelineTest.*"
./module/wgx/tools/validate_spirv_smoke.sh out/cmake_host_build
```

### Current observed passing baseline

At the time this document was updated:

1. `WgxSpirvSmokeTest.*`: 77 passed, 0 failed
2. `WgxVulkanPipelineTest.*`: 4 passed, 0 failed

The Vulkan pipeline tests currently cover:

1. struct interface shaders
2. descriptor-set / uniform mapped shaders
3. texture + sampler mapped shaders
4. vertex attribute input shaders

### Practical conclusion from the current baseline

The backend is already sufficient for:

1. generating valid SPIR-V for a narrow graphics subset
2. creating real Vulkan shader modules and graphics pipelines for that subset
3. validating basic non-compute graphics rendering paths

The backend is not yet sufficient for:

1. covering the full shader feature surface already used by Skity's real WGSL
   generators
2. claiming broad WGSL graphics-shader compatibility

## What Still Blocks Repository-Style Rendering Shaders

The main blocker is no longer resource binding or entry-point plumbing.
The main blocker is expression and math surface area.

Repository shaders already use operations such as:

1. matrix multiply chains beyond the currently validated slice
2. math builtins such as:
   - `min`
   - `floor`
   - `ceil`
   - `round`
   - and broader forms of already-started builtins when repository shaders
     require them
3. richer matrix/vector construction and use in more complex expressions
4. some geometry paths also use bit-style manipulation patterns

That means the current backend can already satisfy a basic graphics-pipeline
slice, but it is still short of the expression surface needed by many existing
Skity rendering shaders.

## Recommended Next Work

Do not widen texture kinds first.
Do not resume broad architectural refactoring first.
The next work should be driven by the real repository shaders that are still
blocked.

### Priority 1: unblock current rendering shaders

Implement these first:

1. math builtins:
   - `min`
   - `floor`
   - `ceil`
   - `round`
   - remaining forms of `abs` / `sign` / `max` / `clamp` if non-float slices
     become necessary
   - broader forms of `select` if vector-bool masks are required

### Priority 2: widen the expression surface around the same shaders

After the first list is stable, extend:

1. `mat3x3<f32> * mat3x3<f32>` if conical-gradient style transforms require it
2. `vec * mat` if a real shader path needs it
3. vector or matrix indexing paths that current shaders depend on
4. additional common math builtins:
   - `smoothstep`
   - `step`
   - `min` if integer/unsigned slices become necessary
5. bit/shift operators if still needed by geometry shaders
6. deeper aggregate validation for struct-heavy expressions

### Priority 3: broaden runtime feature surface

Only after the expression/math slice is stable:

1. expand texture builtin coverage
2. expand texture type coverage beyond `texture_2d<f32>`
3. expand builtin IO surface
4. add more Vulkan runtime cases beyond the current happy-path pipelines

## Recommended Implementation Order

If continuing immediately from the current state, use this order:

1. add `min`, `floor`, `ceil`, and `round`
2. add broader `select` forms if real shaders require vector-bool masks
3. add `mat3x3<f32> * mat3x3<f32>` if needed by repository shaders
4. add any still-needed bit/shift operations

This order matches the current repository shader pressure better than jumping
straight to more texture forms.

## Suggested Validation For The Next Slice

For each newly added operator or builtin:

1. add the smallest focused `WgxSpirvSmokeTest.*` case first
2. validate with `spirv-val` and `spirv-dis`
3. add or expand Vulkan pipeline coverage when the feature participates in a
   real vertex/fragment pipeline path
4. prefer repository-style shader snippets over synthetic math-only examples
   when choosing follow-up integration tests

Good immediate validation targets are:

1. texture-style vertex transforms using `mat4x4 * mat4x4 * vec4`
2. gradient-style fragment math using `dot` or `distance`
3. rrect-style geometry snippets using `sqrt`, `abs`, `sign`, `max`, `mix`,
   and `select`
4. text / SDF snippets using `inversesqrt`, `length`, and `normalize`

## Builtin Mapping Rule

For new builtin work, prefer this mapping rule:

1. use SPIR-V core instructions when there is a direct opcode:
   - arithmetic / comparisons
   - `OpDot`
   - matrix multiply ops
   - cast ops
   - image/sample ops
2. use `OpExtInstImport "GLSL.std.450"` when the builtin is primarily a
   math-library operation with no better direct core opcode:
   - `sqrt`
   - `abs`
   - `sign`
   - `max`
   - `clamp`
   - `mix`
   - `atan`
   - `length`
   - `normalize`
   - `inversesqrt`
   - `floor`
   - `ceil`
   - `round`
   - `min`
3. lower to simpler IR when that is materially simpler or reuses already-tested
   pieces:
   - current `distance` lowers to `sub + dot + sqrt`

## Practical Rule For Future Agents

If you are continuing work in this area:

1. use `out/cmake_host_build`
2. keep the current `texture_2d<f32>` / `sampler` path stable
3. prioritize expression/math support before more texture kinds
4. prefer real repository shader pressure over synthetic feature expansion
5. keep validation narrow and incremental before broadening the test matrix
