# Skity Development

Skity organized the rendering around with [`skity::Canvas`](../include/skity/render/canvas.hpp) 、[`skity::Paint`](../include/skity/graphic/paint.hpp) and [`skity::Path`](../include/skity/graphic/path.hpp).
They describe the rendering area, the geometry to be drawn, and how to fill it with color.

## Canvas

`skity::Canvas` is the core class of Skity. In addition to the drawing area, it also saves the transformation and clipping state of the rendering.
It allows you to draw content on a piece of CPU memory or GPU texture. The details about API of `skity::Canvas` can be found in [canvas.hpp](../include/skity/render/canvas.hpp).

### CPU Raster Canvas

A CPU raster canvas is a canvas that renders content on a piece of CPU memory.
It can be created directly from a [`skity::Bitmap`](../include/skity/graphic/bitmap.hpp) instance.

```c++
skity::Bitmap bitmap(800, 600); // bitmap use RGBA_8888 format and Unpremultiplied alphy type by default.

auto canvas = skity::Canvas::MakeSoftwareCanvas(&bitmap);
```

> Note: Skity CPU raster backend only support RGBA color format for now. 
> Drawing to an bitmap with other color format may have undefined behavior.

### GPU Accelerated Canvas

Currently Skity support OpenGL/GLES and Metal as backend. To use GPU accelerated canvas for rendering.
Needs to create a [`skity::GPUContext`](../include/skity/gpu/gpu_context.hpp) and a [`skity::GPUSurface`](../include/skity/gpu/gpu_surface.hpp) instance first.
The creation of these classes varies, as they are closely tied to the specific GPU API you select.

#### OpenGL/GLES Context Creation

OpenGL related context creation API is in [gpu_context_gl.hpp](../include/skity/gpu/gpu_context_gl.hpp).
Instead of link OpenGL related library at build time. Skity chooses to loading GL symbol functions at runtime.
In this way, if needs to use other OpenGL implementation, such as [angle](https://github.com/google/angle) or [swiftShader](https://github.com/google/swiftshader), no needs to recompile, just replace the GL function.

The OpenGL related `skity::GPUContext` instance can be created like this:

```c++

void* proc_loading_function(const char* name) {
  return dlsym(RTLD_DEFAULT, name);
}

// create OpenGL context
// The proc_loading_function is the function pointer to load OpenGL symbol function.
// It should be the function pointer to `glfwGetProcAddress` or `wglGetProcAddress` or `eglGetProcAddress` etc.
std::unique_ptr<skity::GPUContext> gpu_ctx = skity::GLContextCreate((void*) proc_loading_function);

```
The `skity::GPUContext` instance can be used to create the `skity::GPUSurface` instance by using this function:

```c++
  /**
   * Create a GPU backend surface for rendering
   *
   * @param desc describe the information to create the surface
   *             different backends may have different descriptor structures
   * @return GPUSurface instance or null if init failed
   */
  virtual std::unique_ptr<GPUSurface> CreateSurface(GPUSurfaceDescriptor* desc) = 0;
```

#### OpenGL/GLES Surface Creation

Skity support create the `skity::GPUSurface` instance from a [OpenGL texture](https://www.khronos.org/opengl/wiki/texture) or from a [OpenGL FrameBuffer Object](https://www.khronos.org/opengl/wiki/Framebuffer_Object). By using the `skity::GPUSurfaceDescriptorGL` structure.

```c++
skity::GPUSurfaceDescriptorGL desc{};
// common fields
desc.backend = skity::GPUBackendType::kOpenGL;
desc.width = 800;
desc.height = 600;
desc.sample_count = 4;
desc.content_scale = 1.f;

// texture surface fields
desc.surface_type = skity::GLSurfaceType::kTexture;
desc.gl_id = texture_id;

// fbo surface fields
desc.surface_type = skity::GLSurfaceType::kFramebuffer;
desc.gl_id = fbo_id;
desc.has_stencil_attachment = false; // If fbo has stencil attachment

```

#### Metal Context Creation

The Metal related context creation API is in [gpu_context_mtl.h](../include/skity/gpu/gpu_context_mtl.h).
> Note: This is an Objective-C++ header file. It cannot be included directly or indirectly by a pure C++ file.

The Metal related context can be created like this:

```c++
id<MTLDevice> device = ...;
id<MTLCommandQueue> command_queue = ...;

// device and command queue can be nil, if do not needs them outside skity engine.
// If device is nil, skity will create a default device.
// If command_queue is nil, skity will create a default command queue.
std::unique_ptr<skity::GPUContext> gpu_ctx = skity::MetalContextCreate(device, command_queue);
```

Users can create `id<MTLDevice>` and `id<MTLCommandQueue>` instances by themselves, or pass `nil` parameters to let skity create them.

#### Metal Surface Creation

Skity support create the `skity::GPUSurface` instance from a [Metal texture](https://developer.apple.com/documentation/metal/mtltexture) or from a [CAMetalLayer](https://developer.apple.com/documentation/quartzcore/cametallayer). By using the `skity::GPUSurfaceDescriptorMTL` structure.

```
skity::GPUSurfaceDescriptorMTL desc{};
// common fields
desc.backend = skity::GPUBackendType::kMetal;
desc.width = 800;
desc.height = 600;
desc.sample_count = 4;
desc.content_scale = 1.f;

// texture surface fields
desc.surface_type = skity::MetalSurfaceType::kTexture;
desc.texture = texture;

// layer surface fields
desc.surface_type = skity::MetalSurfaceType::kLayer;
desc.layer = layer;

```

If create `skity::GPUSurface` from `CAMetalLayer`, skity will handle the drawable presentation itself.
But we recommend to use `id<MTLTexture>` to create `skity::GPUSurface` instance. Because skity cannot perceive the size change of CALayer in time, this will cause the size of [`MetalDrawable`](https://developer.apple.com/documentation/metal/mtldrawable) to be updated untimely, causing the drawn content to be stretched.

### Drawing with GPUSurface

Once the `skity::GPUSurface` instance is created, we can use it to get the `skity::Canvas` instance associated with it and submit draw commands.

```c++

auto canvas = surface->LockCanvas();

canvas->DrawColor(0xff000000);

canvas->Flush(); // submit draw commands to GPU, this function must be called before `surface->Flush()`

surface->Flush(); // present rendering result to the GPU target like texture

```

In every frame, call `surface->LockCanvas()` to obtain the `skity::Canvas` instance, then call the draw function.
In the end of every frame, call `canvas->Flush()` first to submit all draw commands to GPU. Then call `surface->Flush()` to present the rendering result.

> Note: 
> * Do not persist the `skity::Canvas` instance. It may changed after `surface->Flush()`
> * Do not call `canvas->Flush()` after `surface->Flush()`.
