# Skity C-API Design

This document describes the design of Skity's C API — a stable, Vulkan-style
ABI surface exposed by `libskity-capi.so`, intended to eventually replace the
direct C++ symbol export of `libskity.so`.

> Status: **normal shape — independent `libskity-capi.so` alongside the C++
> API; the unified shape (`SKITY_UNIFIED_LIB`) ships the C ABI inside
> `libskity` with the legacy C++ exports hidden.** The existing C++ API under
> [`include/skity/`](../../include/skity/skity.hpp) is untouched in the
> normal shape and remains the primary interface for current consumers.

## 1. Motivation & Goals

Skity's public C++ headers cross the shared-library boundary with types whose
layout depends on the C++ compiler and standard library:

- `std::unique_ptr` / `std::shared_ptr` return values
  ([`GLContextCreate`](../../include/skity/gpu/gpu_context_gl.hpp),
  [`GPUContext::CreateSurface`](../../include/skity/gpu/gpu_context.hpp),
  [`Shader::MakeLinear`](../../include/skity/effect/shader.hpp), …);
- virtual dispatch
  ([`GPUContext`](../../include/skity/gpu/gpu_context.hpp),
  [`GPUSurface`](../../include/skity/gpu/gpu_surface.hpp) are abstract bases);
- `std::vector` / `std::string` / `std::function`
  ([`Path`](../../include/skity/graphic/path.hpp),
  [`Typeface`](../../include/skity/text/typeface.hpp), …).

Because of this, linking against `libskity.so` from a different libstdc++ /
libc++ flavor, a different compiler major version, or — most importantly — from
a non-C++ language (Swift, Rust, Go, C#) is unsafe.

### Goals

- Provide a **C ABI surface** that is stable across compilers, standard
  libraries, and languages.
- Make the surface **extensible** without breaking binary compatibility, using
  the Vulkan `sType` / `pNext` chaining pattern.
- Add **runtime type safety** to opaque handles so that a mistaken cast between
  handle types is rejected instead of corrupting memory.

### Non-goals

- Reducing `libskity.so`'s own binary size. The C wrapper is a thin forwarding
  layer; the core library still ships in full. (The unified shape hides the
  C++ exports but does not shrink the core — see §2.)
- Replacing the C++ API in one step. Existing consumers keep the normal
  shape throughout the migration.

## 2. Terminal Architecture

Two mutually exclusive build shapes (`SKITY_UNIFIED_LIB`, default OFF):

```
normal (default)
libskity.so              exports the C++ API (+ render core)
libskity-capi.so         exports the skity_* C ABI
include/skity/           legacy C++ headers (installed)

unified (-DSKITY_UNIFIED_LIB=ON)
libskity.so              exports ONLY the skity_* C ABI (C++ symbols hidden)
libskity-codec.so        exports ONLY its skity_codec_* C ABI
include/skity/c/*.h      pure C headers: handles + CreateInfo + POD types
include/skity/*.hpp     header-only: RAII wrappers over C handles (a la vulkan.hpp;
                         paths spell EXACTLY like the legacy headers;
                         source root: module/capi/wrapper/include)
```

In both shapes the ABI is stable because only **C handles + POD structs** flow
across the `.so` boundary. Every C++ type (smart pointers, containers, vtables)
lives in the consumer's translation unit — in the unified shape through the
header-only wrapper layer, compiled with the consumer's own toolchain —
so it never crosses the boundary at all.

This is the same split Khronos uses between `vulkan.h` (C) and `vulkan.hpp`
(C++ wrapper).

## 3. Handle Model

C++ objects are **not** exposed by `reinterpret_cast`-ing them directly — that
would couple the handle layout to the C++ class layout and break the moment the
class gains a member. Instead, the wrapper layer wraps each object in a small
struct whose **first member is a shared header**:

```c
/* skity/c/skity_base.h */
typedef enum {
    SKITY_OBJECT_TYPE_CONTEXT = 1,
    SKITY_OBJECT_TYPE_SURFACE,
    SKITY_OBJECT_TYPE_CANVAS,
    SKITY_OBJECT_TYPE_PAINT,
    SKITY_OBJECT_TYPE_PATH,
    SKITY_OBJECT_TYPE_SHADER,
    SKITY_OBJECT_TYPE_COLOR_FILTER,
    SKITY_OBJECT_TYPE_IMAGE_FILTER,
    SKITY_OBJECT_TYPE_MASK_FILTER,
    SKITY_OBJECT_TYPE_PATH_EFFECT,
    SKITY_OBJECT_TYPE_TYPEFACE,
    SKITY_OBJECT_TYPE_FONT_MANAGER,
} skity_object_type;

#define SKITY_HANDLE_OWNING 0x1u

typedef struct skity_object_header {
    uint32_t type;   /* skity_object_type — rejects wrong-type casts  */
    uint32_t flags;  /* SKITY_HANDLE_OWNING bit + reserved             */
} skity_object_header;
```

The `header` being the first member lets the wrapper treat any handle as a
`skity_object_header*` to validate its type before use:

```cpp
/* internal to the wrapper (common/handle.hpp) */
template <typename W>
W* resolve(void* h, skity_object_type expected) {
    auto* hdr = static_cast<skity_object_header*>(h);
    return (hdr && hdr->type == expected) ? static_cast<W*>(h) : nullptr;
}
```

Each concrete handle is header + an `impl` pointer to the real C++ object. The
`impl` type is visible only inside the wrapper; C users see a fully opaque
pointer:

```c
SKITY_C_DEFINE_HANDLE(skity_paint);   /* typedef struct skity_paint_s* skity_paint */
```

### Ownership

The wrapper holds the underlying object as a type-erased
`std::shared_ptr<void> impl`. Owning handles store a shared_ptr whose deleter
releases the object; non-owning handles store the raw pointer with a no-op
deleter. Destruction is therefore uniform — the deleter alone decides whether
the object is touched:

```cpp
/* common/handle.hpp */
template <typename Wrapper>
inline void destroy_handle(void* handle, skity_object_type expected) {
    auto* w = resolve<Wrapper>(handle, expected);
    if (!w) return;   /* null or wrong type → reject */
    delete w;         /* shared_ptr impl: deleter releases the underlying
                       * object iff the handle is owning */
}
```

This resolves the most common lifetime trap: the `Canvas*` returned by
[`GPUSurface::LockCanvas()`](../../include/skity/gpu/gpu_surface.hpp) is owned by
the surface. Its handle is built with a no-op deleter
(`std::shared_ptr<void>(raw, [](void*){})`), so `skity_canvas_destroy` only
reclaims the wrapper struct and never touches the surface's object. A C caller
can call `*_destroy` uniformly without tracking which handles are owning.

## 4. CreateInfo & Extensibility

Two independent mechanisms are kept strictly separate:

- **CreateInfo descriptors** (short-lived, on the stack) use `s_type` / `p_next`;
- **handle objects** (long-lived, on the heap) use `header.type` / `header.flags`.

Skity's C++ inheritance `GPUSurfaceDescriptor` → `GPUSurfaceDescriptorGL` maps
naturally onto a base CreateInfo with a backend-specific struct hung off
`p_next`. Adding a field or a backend later only adds a new structure type — it
never breaks the existing ABI:

```c
typedef enum {
    SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO,
    SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL,
    /* future fields/backends add new values here */
} skity_structure_type;

typedef struct skity_surface_create_info {
    skity_structure_type s_type;
    const void*          p_next;   /* → skity_surface_create_info_gl, … */
    uint32_t width;
    uint32_t height;
    uint32_t sample_count;
    float    content_scale;
} skity_surface_create_info;

typedef struct skity_surface_create_info_gl {
    skity_structure_type  s_type;
    const void*           p_next;
    skity_gl_surface_type surface_type; /* TEXTURE or FRAMEBUFFER */
    uint32_t              gl_id;        /* GL texture id or framebuffer id
                                         * (0 = on-screen default FBO) */
    uint32_t              has_stencil;  /* valid only for FRAMEBUFFER */
} skity_surface_create_info_gl;
```

## 5. Type Mapping

| C++ type | Category | C representation |
|---|---|---|
| [`GPUContext`](../../include/skity/gpu/gpu_context.hpp) | opaque, owning handle | `skity_context` |
| [`GPUSurface`](../../include/skity/gpu/gpu_surface.hpp) | opaque, owning handle | `skity_surface` |
| [`Canvas`](../../include/skity/render/canvas.hpp) | opaque, **non-owning** handle | `skity_canvas` |
| [`Paint`](../../include/skity/graphic/paint.hpp) | opaque, owning handle | `skity_paint` |
| [`Path`](../../include/skity/graphic/path.hpp) | opaque, owning handle | `skity_path` |
| [`Shader`](../../include/skity/effect/shader.hpp) / [`ColorFilter`](../../include/skity/effect/color_filter.hpp) / [`ImageFilter`](../../include/skity/effect/image_filter.hpp) / [`MaskFilter`](../../include/skity/effect/mask_filter.hpp) / [`PathEffect`](../../include/skity/effect/path_effect.hpp) | opaque, owning handle | `skity_shader` / … |
| [`Typeface`](../../include/skity/text/typeface.hpp) / [`FontManager`](../../include/skity/text/font_manager.hpp) | opaque, owning handle | `skity_typeface` / `skity_font_manager` |
| [`Rect`](../../include/skity/geometry/rect.hpp) | POD | `skity_rect { float left, top, right, bottom; }` |
| [`Vec4`](../../include/skity/geometry/vector.hpp) / `Point` / `Color4f` | POD (4 floats) | `skity_vec4 { float e[4]; }` |
| [`Vec2`](../../include/skity/geometry/vector.hpp) | POD (2 floats) | `skity_vec2 { float e[2]; }` |
| [`Matrix`](../../include/skity/geometry/matrix.hpp) | POD (16 floats, column-major) | `skity_matrix { float m[16]; }` |
| `Color` / `PMColor` | `uint32_t` (ARGB) | `uint32_t` |
| [`BlendMode`](../../include/skity/graphic/blend_mode.hpp) | `int32_t` enum | `skity_blend_mode` (values aligned) |
| [`TileMode`](../../include/skity/graphic/tile_mode.hpp) | enum | `skity_tile_mode` |
| `GlyphID` | `uint16_t` | `uint16_t` |

All POD structs are standard-layout and binary-compatible with their C++
counterparts — they may be passed by pointer across the boundary with no
conversion.

> `RRect` contains a `std::array` and is not pure POD, so it is not exposed as a
> struct. APIs that need rounded-rectangle geometry take a `skity_rect` plus an
> explicit radii parameter.

## 6. Core API Surface

All functions return [`skity_result`](./include/skity/c/skity_base.h)
(`SKITY_SUCCESS = 0`, `SKITY_ERROR_INVALID_HANDLE`,
`SKITY_ERROR_INITIALIZATION_FAILED`, …). Every `extern "C"` function is
`noexcept`; the wrapper catches C++ exceptions and converts them to a result.

```c
/* context — GL entry */
skity_result skity_context_create_gl(skity_gl_get_proc get_proc,
                                     skity_context* out_context);
void         skity_context_destroy(skity_context context);

/* surface — base CreateInfo + p_next extension */
skity_result skity_surface_create(skity_context ctx,
                                  const skity_surface_create_info* info,
                                  skity_surface* out_surface);
void         skity_surface_destroy(skity_surface surface);
void         skity_surface_flush(skity_surface surface);

/* canvas — non-owning, lifetime tied to the surface */
skity_canvas skity_surface_lock_canvas(skity_surface surface, uint32_t clear);
skity_result skity_canvas_flush(skity_canvas canvas);
void         skity_canvas_draw_rect(skity_canvas c, const skity_rect* r,
                                    skity_paint paint);
void         skity_canvas_draw_path(skity_canvas c, skity_path path,
                                    skity_paint paint);
void         skity_canvas_concat(skity_canvas c, const skity_matrix* m);

/* paint — owning */
skity_paint  skity_paint_create();
void         skity_paint_destroy(skity_paint paint);
void         skity_paint_set_color(skity_paint p, uint32_t color);
void         skity_paint_set_style(skity_paint p, skity_paint_style style);
void         skity_paint_set_shader(skity_paint p, skity_shader shader);
void         skity_paint_set_anti_alias(skity_paint p, uint32_t aa);

/* shader factory — owning */
skity_shader skity_shader_make_linear(const skity_vec4 pts[2],
                                      const skity_vec4* colors,
                                      const float* pos, int32_t count,
                                      skity_tile_mode mode);
```

## 7. Backend Bindings

### OpenGL / OpenGL ES

The C++ [`GLContextCreate(void* proc_loader)`](../../include/skity/gpu/gpu_context_gl.hpp)
does **not** take a C++ `GLProcLoader` object. Despite the documentation, the
`void*` is in fact a glad `GLADloadfunc` — a plain C function pointer
`void* (*)(const char*)` (see `third_party/glad/include/glad/gl.h`). Example
[`example/common/gl/window_gl.cc`](../../example/common/gl/window_gl.cc) passes
`glfwGetProcAddress` directly.

The C API therefore takes a single function pointer:

```c
typedef void* (*skity_gl_get_proc)(const char* name);
skity_result skity_context_create_gl(skity_gl_get_proc get_proc,
                                     skity_context* out_context);
```

### Vulkan

Vulkan types (`PFN_vkGetInstanceProcAddr`, `VkInstance`, …) are already C and
ABI-stable, so the C API reuses them directly rather than redefining wrappers.
The simple constructor mirrors the engine-created overload of
[`CreateGPUContextVK`](../../include/skity/gpu/gpu_context_vk.hpp):

```c
skity_result skity_context_create_vk(PFN_vkGetInstanceProcAddr get_proc,
                                     skity_context* out_context);
```

For applications that need to share their Vulkan instance, device, queues, and
extension sets with skity, `skity_context_vk.h` also declares
`skity_context_create_vk_ex` and `skity_context_create_info_vk`. Vulkan-only
surface and texture wrappers are declared in `skity_surface_vk.h` and
`skity_texture_vk.h` so non-Vulkan consumers do not include
`<vulkan/vulkan.h>`. Semaphores are backend-neutral
(`skity_semaphore.h`: `skity_semaphore_create` / `skity_semaphore_import` /
`skity_semaphore_destroy` / `skity_semaphore_get_backend_type`, mirroring
`GPUContext::CreateSemaphore` / `ImportSemaphore`); only the import-info
extension (`skity_semaphore_import_info_vk`, a plain sync-fd struct) lives in
`skity_semaphore_vk.h` and pulls in no Vulkan types.

### Metal (Stage 2)

[`gpu_context_mtl.h`](../../include/skity/gpu/gpu_context_mtl.h) is an Objective-C++
header. Its C wrapper lives in `skity_context_mtl.h` (+ `skity_surface_mtl.h` /
`skity_texture_mtl.h`) and types the native `id<MTLDevice>` /
`id<MTLCommandQueue>` / `id<MTLTexture>` / `CAMetalLayer*` handles through
`skity_mtl_types.h`: the real Objective-C types, always — the Metal C API
is Objective-C-only with no plain-C / C++ `void*` fallback. Like the Vulkan
headers, the Metal headers are not part of the `skity.h` umbrella —
Objective-C(++) consumers include them directly, and including one from a
non-ObjC TU is a compile `#error`. The typed entry points therefore live in
`mtl_glue.mm`, while portable stubs in `context_c.cpp` return
`SKITY_ERROR_NOT_SUPPORTED` on builds without `SKITY_MTL_BACKEND`.

## 8. Header-only C++ Wrapper

For C++ consumers, the unified shape's header-only wrapper layer wraps the C
handles in RAII types that are binary-identical in usage to today's classes.
Because it is header-only, it is compiled with the consumer's own toolchain
and may freely use `std::shared_ptr` / `std::vector` without introducing any
ABI coupling — those types never enter `libskity.so`.

```cpp
/* skity.hpp — header-only, compiled in the consumer's TU */
namespace skity {
class Canvas {  /* non-owning view, destructor does not delete */
 public:
  void DrawRect(const Rect& r, const Paint& p) {
    skity_canvas_draw_rect(h_, &r, p.get());
  }
  void Flush() { skity_canvas_flush(h_); }
 private:
  skity_canvas h_ = nullptr;
  friend class Surface;
};
class Paint {  /* owning, destructor calls skity_paint_destroy */
 public:
  ~Paint() { skity_paint_destroy(h_); }
  skity_paint get() const { return h_; }
 private:
  skity_paint h_;
};
}  // namespace skity
```

### 8.1 Why the wrapper is unified-build-only

The wrapper defines **plain `namespace skity`** classes with the legacy
names and signatures (see the sample above) — which is only sound because
the unified library exports nothing but the C ABI. Against a normal
`libskity.so`, which still exports the legacy C++ API (the `SKITY_DLL`
visibility leak — hundreds of symbols), a wrapper member call that is not
inlined mangles to the same symbol as the legacy C++ class method; the
dynamic linker binds the call to the legacy implementation, which then runs
with the wrapper object as `this` and silently corrupts it (verified with a
watchpoint: `Paint::SetStrokeWidth(2.f)` wrote the 2.0f bit pattern into
the high half of `handle_`, at `-O0` only — `-O2` inlined everything and
appeared fine).

So instead of a staging namespace, the wrapper ships **only** with the
unified shape (`SKITY_UNIFIED_LIB=ON`): not installed in the normal build,
guarded by `#error` in `base.hpp` unless `SKITY_UNIFIED_LIB` is defined,
and its tests live in the unified-only `skity_wrapper_unit_test` binary
rather than in `skity_unit_test`. A TU can never include both the wrapper
and the legacy headers — that is a redefinition, and in the unified shape
the legacy surface does not link anyway.

### 8.2 Why vulkan.hpp does not need this guard

vulkan.hpp has no such problem, because its world already satisfies our
unified end state:

- `libvulkan` (loader, layers, drivers) exports **C symbols only** — there
  has never been an official C++ ABI to collide with. An uninlined
  `vk::`-wrapper call emits a weak inline symbol that the *static* linker
  merges across TUs; the dynamic linker has no exported surface to bind to.
- `vk::raii` exists for a different reason: three ownership models coexist
  long-term — plain `vk::` handle wrappers (destructor does not destroy, like
  our `Canvas` view), the legacy `vk::UniqueHandle` (deleter storage per
  object), and `vk::raii` (move-only, created from a dispatcher-owning
  context, zero deleter overhead). That layering is permanent.

We adopt the *precondition* rather than borrowing a namespace: the
vulkan.hpp naming is only sound once the library exports nothing but C —
hiding the legacy C++ symbols is not optional polish, it is what makes the
wrapper's naming safe, which is exactly what `SKITY_UNIFIED_LIB` enforces.

Engineering conventions worth keeping from vulkan.hpp: a single configurable
`VULKAN_HPP_INLINE`-style knob if we ever need to control inline fallback, a
`detail` namespace for non-API entities (we have
`skity::detail::OwnHandle`), and the discipline of **zero out-of-line
symbols** in the header-only layer — the few vulkan.hpp needs are gated by
`VULKAN_HPP_STORAGE_API`; our wrapper is pure forwarding and should stay
that way.

### 8.3 Wrapper coverage

One wrapper header per C domain header (`skity/c/skity_paint.h` →
`skity/graphic/paint.hpp` — the legacy header's own path), aggregated by
`skity/skity.hpp`. Consumer include lines are therefore IDENTICAL across the
two shapes; the build shape decides which tree resolves them. Following
the coverage discipline of the C layer, the wrapper only forwards existing
entry points — helper methods that had no C counterpart (Rect / Matrix
arithmetic, color packing) are implemented inline in `types.hpp`.
`test/ut/capi/wrapper_hpp_test.cc` is the compile + CPU smoke gate.

Coverage (2026-10, counted mechanically over `SKITY_C_API` declarations):
397 C functions across the headers under `skity/c/`. The wrapper layer
forwards 393 of them (98%) — everything except the 4 precompile entry
points (below). Only 2 of those are VK-specific functions behind the
opt-in `skity_vk.hpp` (context creation — `skity_context_vk`); all the
rest live in the neutral `skity.hpp` umbrella — except Metal, which has NO wrapper
at all: like the legacy C++ API (whose only Metal header was the
Objective-C++ `gpu_context_mtl.h`), Metal consumers call the C ABI
directly (`skity_context_mtl.h` / `skity_surface_mtl.h` /
`skity_texture_mtl.h`). 20 of the 32 wrapper headers carry handle classes (27
`OwnHandle` classes in total); the rest are POD / free-function / view
layers.

Value-type wrappers mirror their copyable C++ counterparts: `Paint`, `Path`
and `Font` have copy construction / assignment that clones the underlying
object via the C `*_clone` entry points (parameters duplicated, refcounted
children — effects, typeface — shared), while moves stay cheap.

The smart-pointer-managed legacy shapes are kept exactly: every
shared_ptr-returning legacy factory (effects, typefaces, Data, Image,
Texture, Semaphore, FontStyleSet, FontManager, TextBlob) returns
`std::shared_ptr<T>` (nullptr when the C entry yields a null handle — the
legacy null convention), every legacy shared_ptr parameter / getter
(Paint's effects, Font / FontManager / Pixmap hand-offs, Bitmap::GetPixmap,
Image::ScalePixels, Shader::MakeShader) uses the shared_ptr shape, and the
legacy unique_ptr creations map onto unique_ptr: GLContextCreate /
FinishRecording / MakeSoftwareCanvas / CreateSimpleFallbackDelegate
(plus the VK context-surface-window creation helpers); the GPUContext
hub members (CreateSurface / CreateTexture / CreateTextureWithDesc /
WrapTexture / CreateSemaphore) live on Context with their legacy shapes. Legacy code — arrow calls, factory temporaries, null
checks — compiles verbatim; `Canvas::MakeSoftwareCanvas` replaces the
interim `SoftwareCanvas` class. The
underlying wrapper classes stay move-only handle owners; the shared_ptr
controls the wrapper's lifetime, and a null C handle maps to a null
shared_ptr. All other wrappers own uniquely and remain move-only.

| C domain | Wrapper | Notes |
|---|---|---|
| `skity_types` | `types.hpp` | Rect / Matrix / RRect value types (binary-compatible with the C PODs), BlendMode / TileMode / AlphaType / ColorType / SamplingOptions / FilterMode / MipmapMode enums, color helpers |
| `skity_base` | `base.hpp` | `detail::OwnHandle` (move-only owner) |
| `skity_presenter` | `gpu/presenter.hpp` | `GPUPresenter` — the legacy class, neutral like `GPUSurface`: `Context::CreatePresenter` (base info + p_next backend extension, `unique_ptr`), `GPUPresenterStatus`, `GPUSurfaceAcquireResult AcquireNextSurface(desc)`, `Present(unique_ptr<Surface>)`; Vulkan is the only backend today (extension in `skity_presenter_vk.h`). Resize / size queries are C-side additions; `GetPresentMode` has no C entry |
| `skity_camera` | `geometry/camera.hpp` | `Camera` (viewport ctor, position / look-at / dist / rotation setters, `GetCamera` / `GetFixedCamera` view matrices) |
| `skity_quaternion` | `geometry/quaternion.hpp` | free functions `QuaternionEulerToMatrix` / `QuaternionAxisAngleToMatrix` |
| `skity_semaphore` | `gpu/semaphore.hpp` | `Semaphore` (Create on a Context, Import via the untyped base info + backend p_next extension, GetBackendType) — VK implementation only today |
| `skity_canvas` | `render/canvas.hpp` | full draw / state / clip / text / image surface; views borrow (Surface::LockCanvas / recorder), `MakeSoftwareCanvas` returns the legacy `unique_ptr<Canvas>` owning form |
| `skity_paint` | `graphic/paint.hpp` | all setters + getters, effect attach in the legacy `shared_ptr` shapes (null detaches), `shared_ptr` effect/typeface getters, copy semantics (deep-copy params, shared effects) |
| `skity_path` | `graphic/path.hpp` | construction, arc family (tangent / oval / SVG), `add_*` (incl. per-corner radii + `AddMode`), point / verb / conic-weight access, `IsRect` / `IsLine` / `IsEqual`, convexity, segment masks, last-pt family, `CopyWith*`, `Clone` + copy semantics (deep geometry copy) |
| `skity_path_effect` | `effect/path_effect.hpp` | discrete + dash |
| `skity_path_measure` | `graphic/path_measure.hpp` | length / pos-tan / segment / contour advance |
| `skity_path_op` | `graphic/path_op.hpp` | free function `Op(one, two, PathOp, out)` |
| `skity_stroke` | `geometry/stroke.hpp` | free functions `StrokePath` / `QuadPath` |
| `skity_shader` | `effect/shader.hpp` | linear / radial / sweep / two-point conical gradients, image shader, local matrix |
| `skity_color_filter` | `effect/color_filter.hpp` | all factories (`shared_ptr` returns, legacy shape) |
| `skity_mask_filter` | `effect/mask_filter.hpp` | blur (+ `BlurStyle`) |
| `skity_image_filter` | `effect/image_filter.hpp` | all factories (`shared_ptr` returns/params, legacy shape) |
| `skity_data` | `io/data.hpp` | copy / with-proc / from-file / empty + accessors; factories return `shared_ptr` (legacy shape) |
| `skity_bitmap` | `graphic/bitmap.hpp` | `Bitmap` + `Pixmap`; the zero-copy wrap takes / `GetPixmap` returns `shared_ptr<Pixmap>` (legacy shapes) |
| `skity_image` | `graphic/image.hpp` | raster / texture / deferred / promise factories, read / scale pixels |
| `skity_surface` | `gpu/surface.hpp` | `Context::CreateSurface` (`unique_ptr`, the legacy hub member) / lock-canvas / flush / size / read-pixels (`shared_ptr<Pixmap>`, GL CreateInfo path), `AddExternalWaitSemaphore` |
| `skity_context` | `gpu/context.hpp` | `CreateGL` (`unique_ptr`, the legacy GLContextCreate shape), the GPUContext hub members `CreateSurface` / `CreateTexture` / `CreateTextureWithDesc` / `WrapTexture` / `CreateSemaphore` (declared here, defined in the surface / texture / semaphore headers), error callback, all `set_enable_*` tuning knobs, resource cache limit |
| `skity_texture` | `gpu/texture.hpp` | `Texture` (created through the `Context::CreateTexture` / `CreateTextureWithDesc` / `WrapTexture` hub members — `shared_ptr` returns, the legacy shapes, p_next extension chaining on WrapTexture; immediate + deferred upload, size queries) |
| `skity_font` + `skity_text` | `text/text.hpp` | `Typeface` (load / default / unichar→glyph / style / tables / variations / `MakeVariation`; factories and hand-offs return `shared_ptr`, the legacy shape), `TypefaceDelegate` (simple-list + custom-callback fallback), `FontManager` (family enumeration, style sets, match family / style / character), `FontStyleSet`, `Font` (complete: size / scale / skew / hinting / edging / all quality flags / metrics / widths / bounds / make-with-size / `LoadGlyph*` family, copy semantics with shared typeface), `TextBlob` (UTF-8 + delegate + glyph-run build, bounds) |
| `skity_glyph` | `text/glyph.hpp` | `GlyphData` non-owning views over the global glyph cache (metrics / bearings / outline path / bitmap description), `GlyphFormat` / `BitmapFormat` enums, `GlyphBitmap` POD passthrough |
| `skity_recorder` | `recorder/recorder.hpp` | `PictureRecorder` (+ build options, last-op offset) and `DisplayList` (draw / cull-rect draw / bounds / op count / properties / rtree search / per-op paint mutation) |

Only what CANNOT live on the neutral layer stays VK-gated, aggregated by
`skity_vk.hpp` and deliberately kept OUT of `skity.hpp` so GL / CPU
consumers never see `<vulkan/vulkan.h>`:

| C domain | Wrapper | Notes |
|---|---|---|
| `skity_context_vk` | `skity_context_vk.hpp` | free functions `CreateVkContext` ×2 (own instance/device, or caller-provided state via `skity_context_create_info_vk`) — the C entries carry Vulkan-typed signatures, so they cannot become neutral `Context` members |
| `skity_presenter_vk` | `skity_presenter_vk.hpp` | `NativeWindowVk` presenter lifecycle (create / resize / size queries / `AcquireNextSurface` / `Present` consuming a `unique_ptr<Surface>`) — the VK counterpart of the legacy GPUPresenter |

`skity_surface_vk` / `skity_texture_vk` / `skity_semaphore_vk` have NO
wrappers: the CreateInfo structs are already Vulkan-style `p_next`
chains, so consumers chain the extension themselves (setting `s_type`,
plus `backend = SKITY_GPU_BACKEND_TYPE_VULKAN` for backend-texture wraps)
and call the neutral `Context::CreateSurface` / `WrapTexture` /
`Semaphore::Import` — the C headers document the chain recipes.

A VK consumer includes `<skity/skity_vk.hpp>` (which also pulls in the
whole neutral layer). Raw C handles acquired outside the RAII layer can
still be adopted via `Context::Adopt` / `Surface::Adopt`.

Still unwrapped by design: `skity_precompile` only.
`bridge.hpp` is the reverse direction (C++ objects lent INTO C
handles) and is not part of the RAII layer.

## 9. Gradual Migration

The migration is no longer a stage sequence on one library but a build-mode
choice: both shapes below ship side by side and are selected at configure
time (`SKITY_UNIFIED_LIB`, default OFF).

| Shape | `libskity.so` | C API | header-only wrapper (`<skity/...>`) | old `include/skity/` C++ headers | Risk |
|---|---|---|---|---|---|
| normal (default) | exports C++ | `libskity-capi.so` | not installed (guarded by `#error`) | primary, installed | — existing C++ consumers unchanged |
| unified (`SKITY_UNIFIED_LIB=ON`) | C++ symbols hidden, only `skity_*` exported; capi compiled in | inside `libskity.so` | installed, plain `skity::` | not installed | C++ consumers must move to the wrapper (or stay on the normal shape) |

The normal shape never breaks existing C++ consumers; the unified shape is
the terminal architecture of §2, available today for consumers that want a
C-ABI-only library with the RAII wrapper as their C++ surface.

## 10. Build

Normal shape — the C API is a standalone shared library that links
`libskity.so`, mirroring [`module/codec`](../codec/CMakeLists.txt):

```bash
cmake -DSKITY_CAPI_MODULE=ON -DSKITY_GL_BACKEND=ON -DSKITY_VK_BACKEND=ON ...
cmake --build .            # produces libskity-capi.so
```

Symbol check — only `skity_*` C symbols should be exported:

```bash
nm -D --defined-only libskity-capi.so | grep ' T '
```

Unified shape — the C ABI compiles into `libskity` itself, the legacy C++
API is hidden, and the header-only wrapper becomes the C++ surface (see
[README.md](README.md#unified-build-skity_unified_lib)):

```bash
cmake -DSKITY_UNIFIED_LIB=ON -DSKITY_TEST=ON -DSKITY_VK_BACKEND=ON ...
cmake --build .            # libskity.so exports only skity_*; no libskity-capi

nm -gU libskity.dylib | grep -v ' _skity_'   # (macOS) must print nothing
```

With `-DSKITY_CAPI_MODULE=OFF` (the default is ON) the build is identical to
today; `libskity.so` is never modified by the normal shape.

## 11. API Coverage & Known Gaps

This section tracks how completely the C API mirrors the C++ public surface
under [`include/skity/`](../../include/skity/skity.hpp), to guide follow-up
work.

### Fully covered

These modules have complete (or near-complete) C coverage of their core:

- Effects: `color_filter`, `mask_filter`, `path_effect`, `image_filter`
  (all factories)
- `FontManager` + `FontStyleSet`
- `Font`: create (incl. scale/skew ctor, `clone`), typeface/size get/set, all
  rendering-quality switches, `get_metrics`, `make_with_size`,
  `get_widths` + `get_bounds`, and the whole `LoadGlyph*` glyph-data family
  (metrics / path / bitmap / bitmap-info)
- `GlyphData`: non-owning handles over the global glyph cache — metrics /
  bearings / extents getters, outline path borrow, bitmap description
  (buffer / stride / format) via `skity_glyph.h`
- `PictureRecorder` + `DisplayList` (record / replay / cull-rect replay /
  rtree search / properties / per-op paint lookup)
- `PrecompileContext`
- `Texture` (create / create-with-descriptor incl. mipmap / wrap-external /
  upload / deferred-upload)
- `Paint` (all setters + getters incl. `get_color4f` / `get_alpha_f`,
  fill/stroke split colors, effect/typeface attach, SDF-for-small-text and
  font-threshold switches, `clone`)
- `GPUContext` backend queries (`skity_is_gpu_backend_supported`,
  `skity_context_get_backend_type` over the shared `skity_gpu_backend_type`
  enum)
- `GPUContext` (create, `set_error_callback`, all `set_enable_*` tuning,
  precompile, `create_texture`, `wrap_texture`, `set_resource_cache_limit`)
- `GPUNativeWindowVK` / presenter lifecycle (create, resize,
  `acquire_next_surface`, `present` through `skity_presenter_vk`)
- Metal backend (`skity_context_mtl.h` / `skity_surface_mtl.h` /
  `skity_texture_mtl.h`): context create + device / queue getters, and the
  surface / texture-wrap p_next extensions — Objective-C-only handles via
  `skity_mtl_types.h` (no `void*` fallback; not part of the skity.h
  umbrella), implemented by `mtl_glue.mm` when `SKITY_MTL_BACKEND` is on
  (portable stubs in context_c.cpp otherwise)
- `Bitmap` / `Pixmap` (pixel access, buffer wrapping via
  `data_make_with_proc` + `pixmap_create` + `bitmap_create_from_pixmap`,
  `set_color_info`), `image_read_pixels`
- 3D / misc: [`Camera`](./include/skity/c/skity_camera.h),
  [`Quaternion`](./include/skity/c/skity_quaternion.h),
  [`Stroke`](./include/skity/c/skity_stroke.h),
  [`Data`](./include/skity/c/skity_data.h)
- POD types: `color`, `blend_mode`, `tile_mode`, `sampling_options`,
  `alpha_type`, `color_type`, `font_style`, `font_metrics`,
  `variation_coordinate`, `variation_axis`, `font_arguments`

`Paint`, `Path`, `Shader`, `Typeface`, `TextBlob`, `Image`, `GPUSurface`,
`Canvas` all have working core coverage but carry functional gaps — they are
listed in the next table.

### Partially covered (gaps remain)

Priority tags: **P3** is deferred / low-value.

| Module | Covered | Missing (priority) |
|---|---|---|
| `Shader` | gradient factories (linear/radial/sweep/conical) + image shader + `set/get_local_matrix`, `is_opaque`, `as_gradient` (type + geometry + two-pass stops; even distribution synthesized for NULL-pos gradients) | — |
| `GPUSurface` | create, `lock_canvas`, `flush`, `read_pixels`, size getters, GL `surface_mode` + `can_blit_from_target_fbo`, Vulkan image/swapchain-image wrapping, external wait semaphore | **P3**: per-surface CoverageAAMode |
| `Path` | construction, arc family (tangent / oval / SVG), `add_*` (incl. per-corner radii), boolean ops, `PathMeasure`, `transform`, last-pt get/set, `copy_with_matrix/scale`, counts / `get_point` / `get_verb` / `get_conic_weight` / `is_rect` / `is_line` / `is_empty` / `is_finite`, convexity get/set, `get_segment_masks`, `add_path` append/extend modes, `clone`, `is_equal`, `get_last_move_pt` | `GetLastMovePt` has no failure signal on the C++ side (empty path result unspecified, mirrored here) |
| `Image` | 5 factories (incl. the `GPUContext` variant) + `read_pixels` + `scale_pixels` + size getters, `get_alpha_type` / `is_texture_backend` / `get_image_type` / `is_lazy` introspection | **P3**: backend handles (`get_texture`, `get_pixmap`, `get_texture_by_context`) |
| `Typeface` | `make_from_file`, `make_from_data`, `get_default`, `unichars_to_glyphs`/`unichar_to_glyph`, `get_data` (owning `skity_data`), `get_font_style`/`is_bold`/`is_italic`, `get_units_per_em`/`get_unique_id`, `contain_glyph`, `contains_color_table`, raw tables (`count_tables`, `get_table_tags`/`get_table_size`/`get_table_data`), variable fonts (`get_variation_position`/`get_variation_axes`/`make_variation` + `skity_font_arguments`), `get_font_descriptor` (style / collection index / factory id + two-pass family name; the C++ full / PostScript name strings are not projected) | — |
| `TextBlob` | build (UTF-8) + draw, pre-shaped glyph build (`create_from_glyphs`: caller-run shaping → glyph ids + positions), `get_bounds`, `compute_bounds`, `TypefaceDelegate` fallback (ordered-list + custom-fallback-callback) | **P3**: `get_text_run` (read-back — no caller demand found in the animax/clay reverse lookup; the proven need was the build side, now covered); fully custom `BreakTextRun` delegate (caller-driven segmentation) |
| `Canvas` | full draw + state + clip + text/glyphs, `draw_image` sampling overloads, `draw_color4f`, per-corner-radii `draw_rrect` / `clip_rrect_radii` / `draw_drrect_radii`, uniform-radii `clip_rrect` / `draw_drrect`, `get_local_clip_bounds` / `get_global_clip_bounds`, `make_software_canvas` | — |
| `DisplayList` | draw / cull-rect draw / bounds / op_count / properties / rtree search (+ non-overlapping rects) / per-op paint lookup, `begin_recording` with build options | `RecordedOpOffset` is exposed as a plain `int32_t` (round-trips through the public `RecordedOpOffset::Make`) — no opaque set type |
| `GPUContext` | (see Fully covered) | — |

The `bridge.hpp` reverse direction (native C++ objects lent INTO C
handles, e.g. `skity_typeface_from_native`) landed with #505.

### Not yet covered (whole module missing)

| Module | Purpose | Why deferred |
|---|---|---|
| `gpu_context_mtl` / `gpu_context_web` | Metal / WebGPU context | platform backends (Stage 2) |
| `utils` (settings / trace) | config / tracing | minor |

> `GPUContext::create_render_target` / `make_snapshot` are intentionally
> skipped — `MakeSnapshot` consumes a `unique_ptr`, incompatible with the
> shared_ptr handle model. Render-to-texture is served by TEXTURE-type
> surfaces instead.

### Suggested priorities

The original **P0–P2** backlog (paint getters, shader local matrix, GL
`surface_mode`, path arc family, image `scale_pixels`, typeface
`make_from_data`, font `scale_x/skew_x`, text-blob bounds), the
**TypefaceDelegate fallback**, the **DisplayList partial-redraw** increment,
the **typeface metadata / table / variation** queries, and the
**glyph-data loading family** (`LoadGlyph*` + `GlyphData`) have all landed.
The **introspection batch** (paint `get_color4f` / `get_alpha_f` / SDF /
font-threshold, shader `is_opaque` / `as_gradient`, image type / alpha /
lazy / texture-backend queries, per-corner-radii canvas clip + drrect,
`get_global_clip_bounds`, GPUContext backend queries and mipmap texture
creation via `skity_texture_descriptor`, typeface `get_font_descriptor`)
has landed. What remains is **P3** — deferred. Backlog by module:

- **Image**: backend handles (`get_texture`, `get_pixmap`,
  `get_texture_by_context`).
- **TextBlob**: `get_text_run` (+`TextRun` projection — read-back has no
  caller demand; the reverse lookup showed callers need to *inject* shaped
  glyphs, which `skity_text_blob_create_from_glyphs` now covers); a fully
  custom `BreakTextRun` delegate (caller-driven run segmentation — the current
  `skity_typeface_delegate_create_fallback` keeps the built-in policy and only
  overrides the typeface choice).

> `MakeSnapshot` remains blocked on the `unique_ptr`-ownership constraint.
