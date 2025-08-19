# Skity

[![Apache licesed](https://img.shields.io/badge/License-Apache--2.0-cyan?logo=apache)](https://github.com/lynx-family/skity/blob/main/LICENSE)
[![codecov](https://codecov.io/github/lynx-family/skity/graph/badge.svg?token=QTK6TSRIHU)](https://codecov.io/github/lynx-family/skity)
[![Testing](https://github.com/lynx-family/skity/actions/workflows/ci.yml/badge.svg)](https://github.com/lynx-family/skity/actions/workflows/ci.yml)

`Skity` is an open-source 2D graphics library developed in C++. 
It focuses on GPU rendering, providing developers with efficient graphics drawing and rendering capabilities. 
Currently, Skity supports mainstream graphics APIs such as OpenGL, OpenGL ES, and Metal. 
Meanwhile, it offers a functional working software rendering backend as a fallback solution, ensuring stable graphics rendering in various environments.

## Getting Started

### Environment Setup

The project use [habitat](https://github.com/lynx-family/habitat) to manage dependencies.
You can install habitat by following the instructions on the habitat repository.

Aslo use [cmake](https://cmake.org/) to build project. Make sure you have installed cmake on your machine.

### Build and Run

First fetch sources from like this:

```shell
# fetch source
git clone git@github.com:lynx-family/skity.git
cd skity
# fetch binary dependency (ninja, gn .. etc)
./tools/hab sync --target dev

```

Then build the project and example code like this:

```shell
# build
cmake -B out/debug -DSKITY_EXAMPLE=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build out/debug

# run the basic example
./out/debug/example/case/basic_example gl

```

Pass `SKITY_EXAMPLE=ON` to cmake to enable build example code. More options can visit [cmake/Options.cmake](./cmake/Options.cmake).

More details can visit [BUILD_AND_RUN.md](./docs/BUILD_AND_RUN.md).

### Development

The code below shows how to create a `skity::GPUSurface` instance using [GLFW](https://www.glfw.org/) with OpenGL backend. The full code can look at [window_gl.cc](./example/common/gl/window_gl.cc)

```c++
GLFWwindow* window = glfwCreateWindow(800, 600, "Demo", nullptr, nullptr);

std::unique_ptr<skity::GPUContext> gpu_ctx = skity::GLContextCreate((void*)glfwGetProcAddress);

// create a on-screen GPUSurface
skity::GPUSurfaceDescriptorGL desc{};
desc.backend = skity::GPUBackendType::kOpenGL;
desc.width = 800;
desc.height = 600;
desc.sample_count = 4;
desc.content_scale = screen_scale_;

desc.surface_type = skity::GLSurfaceType::kFramebuffer;
desc.gl_id = 0; // render to screen

auto surface = gpu_ctx->CreateSurface(&desc);

auto canvas = surface->LockCanvas();

// canvas->DrawXXX();

// submit render commands
canvas->Flush();

// present surface contents
surface->Flush();

// swap buffers to display
glfwSwapBuffers(window);

```

More details can visit [DEVELOPMENT.md](./docs/DEVELOPMENT.md).

### Drawing Path

```c++
// paint controls the color and style when geometry is rendered
skity::Paint paint;
paint.setStyle(skity::Paint::kFill_Style);
paint.setColor(skity::ColorSetRGB(0x42, 0x85, 0xF4));

// create path
skity::Path path;
path.MoveTo(199, 34);
path.LineTo(253, 143);
path.LineTo(374, 160);
path.LineTo(287, 244);
path.LineTo(307, 365);
path.LineTo(199, 309);
path.LineTo(97, 365);
path.LineTo(112, 245);
path.LineTo(26, 161);
path.LineTo(146, 143);
path.Close();

canvas->DrawPath(path, paint);
```

### Blur Effect

By using [`MaskFilter`](./include/skity/effect/mask_filter.hpp), can make some **Post-processing** effect.
The code below shows how to apply a blur effect to the star path.

```c++
paint.SetMaskFilter(
      skity::MaskFilter::MakeBlur(skity::BlurStyle::kNormal, 10.f));

canvas->DrawPath(path /* previouse created star path */, paint);

```

<p align="center">
  <img src="./docs/resources/blur_star.png"  width="300"/>
</p>



## License

Skity is licensed under the **Apache License, Version 2.0**. See the [LICENSE](./LICENSE) file for more details.
When using Skity for development, please follow the license terms.

## Credits

Skity use the following third-party libraries.
We appreciate the efforts of the developers and the open-source community behind these projects.

- [GLM](https://github.com/g-truc/glm) Used for math operations.
- [Clipper2](https://github.com/AngusJohnson/Clipper2) Used for path calculation.
- [FreeType](https://www.freetype.org/) Used for loading font and rasterizing text.
- [nanopng](https://gitlab.com/TSnake41/nanopng) Used to make freetype as small as possible
- [jsoncpp](https://github.com/open-source-parsers/jsoncpp) Used to parse json file.
- [pugixml](https://github.com/zeux/pugixml) Used to parse xml file.
- [fmt](https://github.com/fmtlib/fmt) Used for string formatting and logging.
- [GLFW](https://www.glfw.org/) Used for window and input in example and test.
- [googletest](https://github.com/google/googletest) Used for testing.
- [google/benchmark](https://github.com/google/benchmark) Used for testing benchmark.


