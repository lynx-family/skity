// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

# WGX IR / SPIR-V Refactor Plan

This document records the main structural risks in the current
`WGSL -> IR -> SPIR-V` pipeline and proposes a prioritized refactor roadmap.

The current implementation is already sufficient for validating small vertical
slices, but it still contains several stage-specific design shortcuts. If the
long-term goal is a robust and extensible WGSL -> IR -> SPIR-V toolchain, the
items below should be addressed before feature growth makes them expensive to
untangle.

## Scope

This document is intentionally not the source of truth for the current active
implementation slice.

Use `module/wgx/docs/IR_DEV.md` for:

- the current recommended next task
- the actively supported backend boundary
- the latest validation baseline

Use this document for:

- long-lived IR design constraints
- refactor priorities that should still guide feature work
- architectural risks that remain relevant even as the active slice changes

## Priority Levels

- **P0**: should be addressed before expanding many new language features
- **P1**: important next-stage refactors that strongly improve scalability
- **P2**: medium-term cleanup / architecture hardening
- **P3**: longer-term structural evolution for broader compiler coverage

---

## P0 - Immediate Structural Risks

### 1. Unify the IR value model

**Problem**

The current IR does not yet have a single uniform representation for values.
Return values, store values, constants, variable references, and SSA results are
encoded in different ways:

- `ReturnValueKind::{kConstVec4F32, kVariableRef, kValueRef}`
- `kStore` payloads encoded through operand shape
- emitter-side materialization logic that infers whether an id means a variable
  or an SSA value

This works for the current narrow subset, but it will scale poorly when adding
more expression forms.

**Why it matters**

Without a unified value model, every new feature tends to create one more
special case in both lowering and SPIR-V emission.

**Recommendation**

Move toward a model where:

- expressions consistently produce IR values
- assignments operate on an address/lvalue target plus a value
- returns consume a value
- constants are represented in a reusable uniform way instead of ad hoc field
  packing

**Expected payoff**

This will simplify arithmetic expansion, constructor expressions, member/index
access, assignment, compound assignment, and later function calls.

---

### 2. Stop using `Instruction` as a mixed expression-result container

**Problem**

Lowering currently uses `ir::Instruction` both as:

- the actual IR instruction object stored in blocks
- a temporary carrier for expression lowering results

That makes expression lowering awkward and mixes persistent IR structure with
transient lowering state.

**Why it matters**

This blocks clean modeling of:

- lvalue vs rvalue
- explicit loads
- constants vs SSA values
- future access chains, calls, constructors, and casts

**Recommendation**

Introduce a dedicated lowering result type such as `ExprResult` that carries:

- resulting type
- category (`value`, `address`, `constant`, or similar)
- referenced id / constant payload

**Expected payoff**

Lowering becomes easier to reason about, and IR generation becomes less coupled
to the current emitter shape.

---

### 3. Make `kLoad` a real part of the IR pipeline

**Problem**

`InstKind::kLoad` exists in the IR enum, but the main lowering/emitter path does
not really use explicit load instructions. Instead, reads from variables are
implicitly materialized inside the SPIR-V emitter.

**Why it matters**

This hides dataflow in the backend and prevents the IR from serving as a clear,
standalone representation of computation.

**Recommendation**

Adopt an explicit model where:

- variable references produce an address/lvalue form
- reading them as expressions emits `kLoad`
- arithmetic, return, and store all work from explicit values

**Expected payoff**

This is the cleanest path toward general expression support and better IR
verification.

---

### 4. Remove bare-id ambiguity between variables and SSA values

**Problem**

Today a plain `uint32_t` id may represent different categories depending on
context. The emitter currently resolves this by probing value maps first and
variable tables second.

**Why it matters**

This is fragile and gets riskier as more IR entities appear.

**Recommendation**

Choose one of the following directions:

- use typed references (`ValueId`, `VarId`, `BlockId`, etc.)
- or move to a single well-defined function-local id space with explicit
  metadata

**Expected payoff**

Fewer accidental category confusions and much safer future IR growth.

---

## P1 - High-Value Next Refactors

### 5. Reduce emitter hardcoding around `vec4<f32>`

**Problem**

Several emitter paths are still effectively specialized around a single value
shape, especially through `vec4_type_id_` and position-output assumptions.

**Why it matters**

This is acceptable for the current milestone, but it will quickly become a
constraint when adding scalars, other vectors, matrices, additional builtins, or
broader IO support.

**Recommendation**

Move the emitter toward type-driven emission:

- derive SPIR-V type ids from each IR value's type
- make materialization helpers type-aware
- avoid assuming that most values are position-style `vec4<f32>`

**Expected payoff**

A cleaner path to general arithmetic and richer type support.

---

### 6. Introduce an IR verifier instead of overloading emitter prechecks

**Problem**

`SupportsCurrentIR()` currently acts as a mixed capability gate and structural
validator.

**Why it matters**

As the IR grows, this style does not scale well. Validation logic becomes harder
to reason about and remains entangled with a specific backend.

**Recommendation**

Create a dedicated IR verifier pass responsible for checking:

- instruction shape validity
- type consistency
- use-def legality
- return correctness
- load/store legality
- later, block/terminator rules

Backend support checks should stay separate from generic IR validity checks.

**Expected payoff**

Cleaner layering and more reliable refactoring.

---

### 7. Decouple lowering from current emitter-specific shapes

**Problem**

The lowering layer currently knows too much about which value shapes the emitter
can consume directly.

**Why it matters**

That reduces the value of IR as an independent middle layer.

**Recommendation**

Push toward a stricter contract:

- lowering produces semantically stable IR
- the emitter only translates that IR into SPIR-V
- backend-specific materialization stays in the backend, not in AST lowering

**Expected payoff**

Feature growth becomes more predictable and less circular.

---

## P2 - Medium-Term Architecture Hardening

### 8. Replace the "large shared Instruction struct" with more structured data

**Problem**

`ir::Instruction` currently carries many fields that are only valid for some
instruction kinds.

**Why it matters**

This makes invalid states easier to create and encourages shape-dependent logic
spread across the pipeline.

**Recommendation**

Evolve the design toward either:

- smaller grouped instruction payloads, or
- a tagged union / variant-style instruction representation

This does not have to happen immediately, but the direction should be explicit.

**Expected payoff**

Safer IR evolution and clearer per-instruction invariants.

---

### 9. Add proper scope-stack handling in lowering

**Problem**

Current local variable tracking is sufficient for the existing straight-line
examples but is not yet robust for nested scopes or shadowing.

**Why it matters**

As soon as blocks become more interesting, a flat lookup model becomes a source
of bugs.

**Recommendation**

Introduce lexical scope push/pop behavior in lowering before broader statement
support lands.

**Expected payoff**

Safer expansion toward block-local declarations and control flow.

---

### 10. Reduce backend-side semantic reconstruction

**Problem**

The emitter still reconstructs some semantics by inspecting operands and local
state instead of consuming explicit IR semantics.

**Why it matters**

This increases backend complexity and can hide bugs in the representation.

**Recommendation**

As the IR is improved, move more meaning out of emitter-side inference and into
explicit instruction/value forms.

**Expected payoff**

A more predictable emitter and easier debugging.

---

## P3 - Longer-Term Compiler Evolution

### 11. Generalize function structure beyond a single entry block

**Problem**

The current function representation is effectively a single linear block.

**Why it matters**

That is enough for the current milestone, but not for a real compiler pipeline.

**Recommendation**

Plan for a future migration toward:

- multiple basic blocks
- explicit terminators
- structured control flow
- later expansion to the remaining WGSL control constructs (`break if`, `switch`)

**Expected payoff**

This is required for non-trivial WGSL programs.

---

### 12. Add a real aggregate/member-access IR model

**Problem**

WGSL struct support can be partially approximated by flattening selected
entry-point interfaces, but that does not create a general model for:

- local struct variables
- helper-function struct parameters and returns
- member access (`a.b`)
- member assignment (`a.b = x`)
- later nested aggregate access

If struct support keeps being implemented through ad hoc flattening or by
mapping members onto unrelated plain local variable slots, then some entry-point
cases may work, but the language's aggregate semantics remain under-modeled.

**Why it matters**

Without an explicit aggregate/member-access model:

- struct support becomes fragmented between entry-point code paths and ordinary
  lowering
- helper-function struct passing needs more special cases
- nested structs and richer aggregate expressions stay awkward to add
- backend code must infer structure that should already be represented in IR

**Recommendation**

Introduce an explicit IR-level aggregate access path. The final instruction
shapes can vary, but the semantics should cover at least:

- struct values as first-class IR types
- struct addresses as first-class lvalues
- a member-address operation for `address-of-struct -> address-of-member`
- a member-extract operation for `value-of-struct -> value-of-member`

Then build higher-level behavior on top of that model:

- assignment uses member addresses
- expression reads use member extracts or loads from member addresses
- helper-function calls can accept and return struct values directly
- entry-point struct IO flattening becomes a boundary transformation rather
  than the main representation of structs

**Expected payoff**

This gives the compiler one coherent story for general structs and makes
entry-point `VertexInput` / `VertexOutput` support a special case of the same
aggregate semantics instead of a separate temporary universe.

---

### 13. Expand validation beyond static SPIR-V legality

**Problem**

The project now validates generated SPIR-V with smoke tests, `spirv-val`, and
`spirv-dis`, which is already a strong step. But this still only guarantees
static legality plus manual structural inspection.

**Recommendation**

In later phases, add optional Vulkan-side integration checks such as:

- shader module creation
- minimal pipeline creation for supported samples

**Expected payoff**

Higher confidence that generated SPIR-V is not only valid, but usable in real
Vulkan environments.

---

## Suggested Refactor Order

The recommended implementation order is:

1. unify value/address modeling
2. introduce `ExprResult`
3. make `kLoad` explicit in the main IR path
4. remove id-category ambiguity
5. make emission type-driven instead of `vec4`-specialized
6. add an IR verifier
7. improve scope handling
8. later expand toward multi-block CFG

## Practical Rule For Future Work

Before adding a new WGSL feature, first ask:

1. should this be represented explicitly in IR semantics?
2. does lowering currently encode backend-specific knowledge?
3. will this new feature create another special case in value materialization?

If the answer to any of these is "yes", prefer a small refactor before adding
more feature-specific branches.
