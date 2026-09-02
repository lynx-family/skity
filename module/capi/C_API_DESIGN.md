# Skity C-API Design

This document describes the design of Skity's C API — a stable, Vulkan-style
ABI surface exposed by `libskity-capi.so`, intended to eventually replace the
direct C++ symbol export of `libskity.so`.

> Status: **Stage 1 — independent `libskity-capi.so`, GL + Vulkan core path.**
> The existing C++ API under [`include/skity/`](../../include/skity/skity.hpp) is
> untouched and remains the primary interface for current consumers.

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
  layer; the core library still ships in full. (See Stage 3 below for the
  eventual size story.)
- Replacing the C++ API in one step. Existing consumers must keep working
  throughout the migration.

## 2. Terminal Architecture

```
libskity.so              exports only extern "C" symbols (C API + render core)
include/skity_c/*.h      pure C headers: handles + CreateInfo + POD types
include/skity.hpp        header-only: RAII wrappers over C handles (a la vulkan.hpp)
```

The ABI is stable because only **C handles + POD structs** flow across the
`.so` boundary. Every C++ type (smart pointers, containers, vtables) lives in
the header-only `skity.hpp`, compiled into the *consumer's* translation unit
with the consumer's own toolchain — so it never crosses the boundary at all.

This is the same split Khronos uses between `vulkan.h` (C) and `vulkan.hpp`
(C++ wrapper).

## 3. Handle Model

C++ objects are **not** exposed by `reinterpret_cast`-ing them directly — that
would couple the handle layout to the C++ class layout and break the moment the
class gains a member. Instead, the wrapper layer wraps each object in a small
struct whose **first member is a shared header**:

```c
/* skity_c/skity_base.h */
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
/* internal to the wrapper (src/handle.hpp) */
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
/* src/handle.hpp */
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

All functions return [`skity_result`](./include/skity_c/skity_base.h)
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
surface, texture, and semaphore wrappers are declared in `skity_surface_vk.h`,
`skity_texture_vk.h`, and `skity_semaphore_vk.h` so non-Vulkan consumers do not
need to include `<vulkan/vulkan.h>`.

### Metal (Stage 2)

[`gpu_context_mtl.h`](../../include/skity/gpu/gpu_context_mtl.h) is an Objective-C++
header. Its C wrapper will live in a `.mm` source and accept the native
`id<MTLDevice>` / `id<MTLCommandQueue>` as opaque `void*`. Not in Stage 1.

## 8. Header-only C++ Wrapper

For C++ consumers, the eventual `skity.hpp` wraps the C handles in RAII types
that are binary-identical in usage to today's classes. Because it is
header-only, it is compiled with the consumer's own toolchain and may freely
use `std::shared_ptr` / `std::vector` without introducing any ABI coupling —
those types never enter `libskity.so`.

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

## 9. Gradual Migration

| Stage | `libskity.so` | C API | header-only `skity.hpp` | old `include/skity/` C++ headers | Risk |
|---|---|---|---|---|---|
| 0 (current) | exports C++ | — | — | primary | — |
| **1 (this work)** | **unchanged** | **`libskity-capi.so`, GL+VK core path** | — | kept | none — C consumers benefit first |
| 2 | unchanged | feature parity with old C++ headers + MTL | new `skity.hpp` shipped | kept / deprecated | old + new C++ entry points coexist |
| 3 (major bump) | C++ symbols hidden, only C exported | complete | primary | removed | requires all downstream migrated |

Each stage is independently shippable and never breaks existing C++ consumers.
Only Stage 3 — hiding the C++ symbols in `libskity.so` — is a breaking change,
and it is gated on downstream migration completion.

## 10. Build

The wrapper is a standalone shared library that links `libskity.so`, mirroring
[`module/codec`](../codec/CMakeLists.txt):

```bash
cmake -DSKITY_CAPI_MODULE=ON -DSKITY_GL_BACKEND=ON -DSKITY_VK_BACKEND=ON ...
cmake --build .            # produces libskity-capi.so
```

Symbol check — only `skity_*` C symbols should be exported:

```bash
nm -D --defined-only libskity-capi.so | grep ' T '
```

With `-DSKITY_CAPI_MODULE=OFF` (the default is ON) the build is identical to
today; `libskity.so` is never modified by this work.

## 11. API Coverage & Known Gaps

This section tracks how completely the C API mirrors the C++ public surface
under [`include/skity/`](../../include/skity/skity.hpp), to guide follow-up
work.

### Fully covered

These modules have complete (or near-complete) C coverage of their core:

- Effects: `color_filter`, `mask_filter`, `path_effect`, `image_filter`
  (all factories)
- `FontManager` + `FontStyleSet`
- `PictureRecorder` + `DisplayList` (record / replay / cull-rect replay /
  rtree search / properties / per-op paint lookup)
- `PrecompileContext`
- `Texture` (create / wrap-external / upload / deferred-upload)
- `GPUContext` (create, `set_error_callback`, all `set_enable_*` tuning,
  precompile, `create_texture`, `wrap_texture`, `set_resource_cache_limit`)
- `Bitmap` / `Pixmap` (pixel access), `image_read_pixels`
- 3D / misc: [`Camera`](./include/skity_c/skity_camera.h),
  [`Quaternion`](./include/skity_c/skity_quaternion.h),
  [`Stroke`](./include/skity_c/skity_stroke.h),
  [`Data`](./include/skity_c/skity_data.h)
- POD types: `color`, `blend_mode`, `tile_mode`, `sampling_options`,
  `alpha_type`, `color_type`, `font_style`, `font_metrics`

`Paint`, `Path`, `Shader`, `Font`, `Typeface`, `TextBlob`, `Image`,
`GPUSurface`, `Canvas` all have working core coverage but carry functional
gaps — they are listed in the next table.

### Partially covered (gaps remain)

Priority tags: **P3** is deferred / low-value.

| Module | Covered | Missing (priority) |
|---|---|---|
| `Paint` | all setters + getters, fill/stroke split colors, effect/typeface attach | **P3**: `get_color4f`, `get_alpha_f`, SDF / font-threshold |
| `Shader` | gradient factories (linear/radial/sweep/conical) + image shader + `set/get_local_matrix` | **P3**: `is_opaque`, `as_gradient` introspection |
| `GPUSurface` | create, `lock_canvas`, `flush`, `read_pixels`, size getters, GL `surface_mode` + `can_blit_from_target_fbo`, Vulkan image/swapchain-image wrapping, external wait semaphore | **P3**: per-surface CoverageAAMode |
| `Path` | construction, arc family (tangent / oval / SVG), `add_*` (incl. per-corner radii), boolean ops, `PathMeasure`, `transform`, last-pt get/set, `copy_with_matrix/scale`, counts / `get_point` / `get_verb` / `get_conic_weight` / `is_rect` / `is_line` / `is_empty` / `is_finite`, convexity get/set, `get_segment_masks`, `add_path` append/extend modes, `clone`, `is_equal`, `get_last_move_pt` | `GetLastMovePt` has no failure signal on the C++ side (empty path result unspecified, mirrored here) |
| `Image` | 5 factories (incl. the `GPUContext` variant) + `read_pixels` + `scale_pixels` + size getters | **P3**: alpha/type/backend introspection |
| `Font` | create (incl. scale/skew ctor), typeface/size get/set, rendering-quality switch get/set, `get_metrics`, `make_with_size`, `get_widths` | **P3**: `get_widths` bounds overload, `LoadGlyph*` (needs GlyphData) |
| `Typeface` | `make_from_file`, `make_from_data`, `get_default`, `unichars_to_glyphs`/`unichar_to_glyph` | **P3**: `get_font_style`/`is_bold`/`is_italic`, `contain_glyph`, `units_per_em`/`contains_color_table`, table / variation / descriptor |
| `TextBlob` | build (UTF-8) + draw, `get_bounds`, `compute_bounds`, `TypefaceDelegate` fallback (ordered-list + custom-fallback-callback) | **P3**: `get_text_run`; fully custom `BreakTextRun` delegate (caller-driven segmentation) |
| `Canvas` | full draw + state + clip + text/glyphs, `draw_image` sampling overloads, `draw_color4f`, per-corner-radii `draw_rrect`, `make_software_canvas` | **P3**: per-corner RRect clip/drrect, `get_global_clip_bounds` |
| `DisplayList` | draw / cull-rect draw / bounds / op_count / properties / rtree search (+ non-overlapping rects) / per-op paint lookup, `begin_recording` with build options | `RecordedOpOffset` is exposed as a plain `int32_t` (round-trips through the public `RecordedOpOffset::Make`) — no opaque set type |
| `GPUContext` | (see Fully covered) | **P3**: `create_texture_with_desc` (mipmap), `is_gpu_backend_supported`, `get_backend_type` |
| `GPUNativeWindowVK` / `GPUPresenter` | Vulkan native-window creation, swapchain resize, surface acquire/present | complete |

### Not yet covered (whole module missing)

| Module | Purpose | Why deferred |
|---|---|---|
| `GPUPresenter` | Vulkan swapchain present (`acquire_next_surface` / `present` / `resize`) through `skity_native_window_vk` | C wrappers consume the acquired `unique_ptr` and explicitly invalidate the surface handle. |
| `GlyphData` | glyph bitmap / path query (used by `Font::LoadGlyph*`) | exposing it cleanly needs a richer query surface |
| `gpu_context_mtl` / `gpu_context_web` | Metal / WebGPU context | platform backends (Stage 2) |
| `utils` (settings / trace) | config / tracing | minor |

> `GPUContext::create_render_target` / `make_snapshot` are intentionally
> skipped — `MakeSnapshot` consumes a `unique_ptr`, incompatible with the
> shared_ptr handle model.

### Suggested priorities

The original **P0–P2** backlog (paint getters, shader local matrix, GL
`surface_mode`, path arc family, image `scale_pixels`, typeface
`make_from_data`, font `scale_x/skew_x`, text-blob bounds) plus the
**TypefaceDelegate fallback** and **DisplayList partial-redraw** increments
have all landed. What remains is **P3** — deferred. Detailed backlog by
module:

- **Paint**: `get_color4f`, `get_alpha_f`; SDF / font-threshold
  (`set/is_sdf_for_small_text`, `get/set_font_threshold`).
- **Shader**: `is_opaque` (opacity hint); `as_gradient` (recover gradient
  params, needs `GradientInfo` / `GradientType`).
- **Path**: convexity (`get/is_convex`, +`ConvexityType`); `get_segment_masks`;
  `get_verb` (+`Verb` enum); `get_last_move_pt`; type queries (`is_line`,
  `is_simple_rrect`, `get_is_a_type`, +`IsAType`); `is_finite`; `reverse_path_to`;
  `add_path` extend mode (+`AddMode`).
- **Image**: property/type introspection (`get_alpha_type`, `is_texture_backend`,
  `get_image_type`, `is_lazy`); backend handles (`get_texture`, `get_pixmap`,
  `get_texture_by_context`).
- **Font**: `get_widths` bounds overload; `LoadGlyph*` family
  (`load_glyph_metrics` / `path` / `bitmap` / `bitmap_info`) — depends on
  `GlyphData`.
- **Typeface**: style (`get_font_style`, `is_bold`, `is_italic`);
  `contain_glyph`; emoji/COLR (`get_units_per_em`, `contains_color_table`);
  raw tables (`count_tables`, `get_table_tags` / `size` / `data`, `get_data`);
  variable fonts (`get_variation_design_position` / `parameters`,
  `make_variation`, +`FontArguments`); cache keys (`typeface_id`,
  `get_font_descriptor`).
- **TextBlob**: `get_text_run` (+`TextRun` projection); a fully custom
  `BreakTextRun` delegate (caller-driven run segmentation — the current
  `skity_typeface_delegate_create_fallback` keeps the built-in policy and only
  overrides the typeface choice).
- **Canvas**: per-corner RRect `clip_rrect` / `draw_drrect`
  (+RRect projection or 8-radii); `get_global_clip_bounds`. (`draw_rrect`
  with per-corner radii, the `draw_image` sampling/paint overloads, and
  `draw_color(color4f, blend)` are covered since the Canvas gap fill.)
- **GPUContext**: `create_texture_with_desc` (mipmap, +`TextureDescriptor`
  mirror); `is_gpu_backend_supported`; `get_backend_type`.

> `MakeSnapshot` remains blocked on the `unique_ptr`-ownership constraint.
> The CPU raster backend (`Canvas::MakeSoftwareCanvas`) is intentionally
> deferred — GPU paths are the current priority.
