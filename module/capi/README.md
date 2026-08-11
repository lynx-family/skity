# skity-capi

A Vulkan-style C ABI wrapper around the core `libskity` library. It links
`libskity.so` and re-exports a stable C surface so that the rendering engine can
be consumed from any language (Swift, Rust, Go, C#) or from C++ built with an
incompatible libstdc++ / libc++, without depending on the unstable C++ ABI.

See [C_API_DESIGN.md](C_API_DESIGN.md) for the full design.

## Build

```
cmake -DSKITY_CAPI_MODULE=ON -DSKITY_GL_BACKEND=ON -DSKITY_VK_BACKEND=ON ...
cmake --build .
```

This produces `libskity-capi.so`. With `-DSKITY_CAPI_MODULE=OFF` the wrapper is
not built and the rest of the project is unaffected.

## Example

A complete, runnable GL example lives with the other skity examples in
[`../../example/case/c_api/main.c`](../../example/case/c_api/main.c). It opens a
GLFW window and drives the full loop through the C API — GL context, GL surface
via the `p_next` extension, paint, gradient shader, path construction, and draw
calls. Build it by enabling the examples:

```
cmake -DSKITY_EXAMPLE=ON -DSKITY_CAPI_MODULE=ON ...
cmake --build . --target skity-c-api
```

## Usage

```c
#include <skity_c/skity.h>

skity_context ctx;
skity_context_create_gl(my_gl_get_proc, &ctx);

skity_surface_create_info_gl gl_ext = {
    .s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO_GL,
    .surface_type = SKITY_GL_SURFACE_TYPE_FRAMEBUFFER,
    .gl_id = fbo,
    .has_stencil = 1,
};
skity_surface_create_info info = {
    .s_type = SKITY_STRUCTURE_TYPE_SURFACE_CREATE_INFO,
    .p_next = &gl_ext,
    .width = 800,
    .height = 600,
};

skity_surface surface;
skity_surface_create(ctx, &info, &surface);

skity_canvas canvas = skity_surface_lock_canvas(surface, 1);
skity_paint paint = skity_paint_create();
skity_paint_set_color(paint, 0xFF000000);

skity_rect r = {10, 10, 200, 200};
skity_canvas_draw_rect(canvas, &r, paint);

skity_canvas_flush(canvas);
skity_surface_flush(surface);

skity_paint_destroy(paint);
skity_canvas_destroy(canvas);
skity_surface_destroy(surface);
skity_context_destroy(ctx);
```
