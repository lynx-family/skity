# WGX Semantic + IR + SPIR-V Roadmap

## Background

`module/wgx` currently lowers WGSL to target code by traversing AST directly in backend printers (`glsl/ast_printer.*`, `msl/ast_printer.*`). This approach is lightweight, but it has structural limits:

1. Identifier resolution is mostly name-string based.
2. Name collision handling is backend-local and ad hoc.
3. Semantic logic (resource capture, builtin mapping, lowering) is duplicated across backends.
4. Adding a new backend (especially SPIR-V) will multiply complexity without a shared middle layer.

This roadmap defines a staged migration to:

1. Semantic binding (scope + symbol resolution).
2. Target-neutral IR.
3. IR-based backend emission, including SPIR-V.

## Goals

1. Fix variable/type/function name collision and shadowing reliably.
2. Keep codegen deterministic and stable across runs.
3. Reduce backend duplication by introducing a shared semantic and lowering pipeline.
4. Enable SPIR-V backend with maintainable complexity.
5. Preserve incremental migration so existing GLSL/MSL outputs keep working during transition.

## Non-goals (initial phases)

1. Full WGSL feature parity in one step.
2. Aggressive optimization passes (CSE, DCE, loop transforms) in early milestones.
3. Rewriting parser/scanner architecture.

## Target Architecture

Pipeline after migration:

1. Parse WGSL -> `ast::Module` (existing parser)
2. Semantic resolve -> `BindResult` (symbols, scopes, diagnostics)
3. Name allocation (target-aware keyword/conflict avoidance)
4. Lower to IR (typed, structured control flow)
5. Emit target code:
   - IR -> GLSL
   - IR -> MSL
   - IR -> SPIR-V

### Proposed directories

1. `module/wgx/semantic/`
2. `module/wgx/ir/`
3. `module/wgx/lower/`
4. `module/wgx/spirv/`

## Phase 1: Semantic Binding

### Deliverables

1. Symbol and scope model.
2. Resolver pass over AST.
3. Diagnostic output for unresolved/duplicate symbols.
4. Side-table mapping from identifier expressions to resolved symbols.

### Key types (proposed)

- `semantic/symbol.h`
  - `enum class SymbolKind { Var, Let, Const, Param, Func, Type, StructMember, Builtin }`
  - `struct Symbol { kind, id, original_name, decl_ptr, type_ref }`
- `semantic/scope.h`
  - lexical scope chain
  - `Declare()`, `Lookup()`
- `semantic/bind_result.h`
  - `unordered_map<const ast::IdentifierExp*, Symbol*> ident_symbol`
  - diagnostics list
- `semantic/resolver.h/.cpp`
  - AST visitor that builds scopes and resolves references

### Resolver behavior

1. Build root scope with builtins and global declarations.
2. Function scope contains parameters and local declarations.
3. Block statements create nested scopes.
4. Declaration order must follow WGSL visibility rules.
5. Identifier usage resolves to nearest visible declaration.

### Acceptance criteria

1. Correct shadowing resolution (global/param/local same name).
2. Duplicate declaration in same scope is diagnosed.
3. Unresolved identifier is diagnosed.
4. Existing valid shaders still compile for GLSL/MSL path.

## Phase 2: Deterministic Name Allocation

### Deliverables

1. Shared `NameAllocator` with target keyword sets.
2. Symbol-id to emitted-name mapping.
3. Removal of backend-local name hacks where possible.

### Proposed interface

- `semantic/name_allocator.h`
  - `enum class TargetLang { GLSL, MSL, SPIRV }`
  - `Assign(Symbol&) -> emitted_name`
  - `Get(symbol_id) -> emitted_name`

### Rules

1. Prefer original name when valid.
2. Avoid target reserved keywords.
3. Avoid collisions in emission namespace.
4. Deterministic suffixing: `_wgx1`, `_wgx2`, ...

### Acceptance criteria

1. Same input produces stable emitted names.
2. Keyword collisions are resolved consistently.
3. Backend printers no longer depend on fragile hardcoded rename rules.

## Phase 3: Minimal IR Definition

### Deliverables

1. Core IR data model for module/function/block/instruction/type.
2. Decorations/metadata for IO and resources.
3. Structured control flow representation.

### Proposed minimal IR components

- `ir/module.h`
- `ir/function.h`
- `ir/block.h`
- `ir/type.h`
- `ir/value.h`
- `ir/inst.h`
- `ir/decoration.h`

### Initial instruction set (minimal)

1. Constants and composites
2. Local/global variable ops (`alloca`, `load`, `store`)
3. Arithmetic/logical ops
4. Access ops (`index`, `member`, access chain style)
5. Calls
6. Terminators (`return`, `branch`, `cond_branch`)
7. Structured control markers (merge hints)

### Acceptance criteria

1. IR can represent currently supported WGSL subset in wgx.
2. IR graph passes internal verification (block termination, type consistency basic checks).

## Phase 4: WGSL AST -> IR Lowering

### Deliverables

1. Lowering pass using `BindResult`.
2. Builtin mapping and resource semantics moved out of printers.
3. Consistent function/type/global lowering.

### Design notes

1. Lower expression nodes to IR values.
2. Lower statements to instruction sequences + blocks.
3. Preserve structured control intent for later SPIR-V emission.
4. Keep feature flags to compare AST-printer and IR-printer outputs during migration.

### Acceptance criteria

1. Existing sample shaders lower successfully.
2. IR-based GLSL/MSL output is semantically equivalent to legacy output for covered cases.

## Phase 5: SPIR-V Emitter

### Deliverables

1. IR -> SPIR-V emitter (`spirv/emitter.*`).
2. ID allocator and dedup tables for types/constants.
3. Entry point, execution model, decorations, function/body emission.
4. Optional text dump for debugging.

### Emission constraints

1. Structured control flow must produce valid merge instructions.
2. Type and storage class mapping must be explicit.
3. Capability and memory model declaration must match generated ops.

### Validation

1. Validate binaries with `spirv-val` in CI or local script.
2. Add representative golden tests for vertex/fragment pipelines.

### Acceptance criteria

1. Generated SPIR-V passes validator for supported feature set.
2. Shader behavior matches GLSL/MSL outputs on shared tests.

## Migration Strategy

1. Keep legacy AST printers active while introducing semantic pass.
2. Switch identifier emission in GLSL/MSL to symbol-aware naming first.
3. Introduce IR under feature flag (`WGX_USE_IR_BACKEND` suggested).
4. Migrate one backend at a time (GLSL first, then MSL).
5. Add SPIR-V on IR path only.
6. Remove legacy duplicated logic after parity is reached.

## Testing Plan

### Unit tests

1. Resolver scope tests (shadowing, duplicates, unresolved refs).
2. Name allocator determinism and keyword collision tests.
3. IR verifier tests for malformed CFG/type mismatch cases.

### Integration tests

1. WGSL -> GLSL/MSL parity snapshots (legacy vs IR path).
2. WGSL -> SPIR-V validation tests (`spirv-val`).
3. Cross-backend semantic consistency tests for key shaders.

### Recommended test inputs

1. Global/param/local same-name shadowing.
2. Struct member names colliding with locals.
3. Builtin and user function overlap edge cases.
4. Texture/sampler builtins.
5. Control flow-heavy shaders (`if/switch/loop/for/while`).

## Risks and Mitigations

1. Risk: migration complexity breaks current outputs.
   - Mitigation: feature flag + golden tests + phased backend switch.
2. Risk: IR design too large up front.
   - Mitigation: start with minimal IR for currently supported WGSL subset.
3. Risk: duplicated temporary logic across old/new paths.
   - Mitigation: prioritize moving shared logic (name resolution, builtin normalization) into common passes early.

## Suggested Milestones

1. M1 (1-2 weeks): semantic resolver + diagnostics + symbol map.
2. M2 (1 week): shared name allocator integrated into GLSL/MSL printers.
3. M3 (2 weeks): minimal IR + basic lowering for current feature subset.
4. M4 (1-2 weeks): GLSL IR emitter parity.
5. M5 (1-2 weeks): MSL IR emitter parity.
6. M6 (2+ weeks): SPIR-V emitter + validator integration.

## Immediate Next Steps

1. Add `semantic/` folder with `symbol`, `scope`, `resolver`, `bind_result`.
2. Wire resolver invocation into compile/write entry path.
3. Replace direct identifier printing with symbol-based name lookup in GLSL/MSL printers.
4. Add initial resolver and naming tests before IR implementation.
