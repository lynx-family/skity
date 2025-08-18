# Build Skity from source

This document will guide you through the process of building the Skity project from source.

## Prerequisites

The third-party libraries used by Skity is managed by [habitat](https://github.com/lynx-family/habitat). A lightweight dependency management tool developed by [lynx-family](https://github.com/lynx-family). You can install it manually or use the helper script located in [tools/hab](../tools/hab) (Windows user can use [tools/hab.ps1](../tools/hab.ps1)) to sync the dependencies.

The Skity project use [CMake](https://cmake.org/) as the build system. Make sure you have CMake version 3.15 or higher installed on your system.

Although Skity supports CPU rendering, it is recommended to ensure that the GPU environment currently supported by Skity is installed.
This is the support GPU backends on different platforms:
* Windows and Linux:
    * OpenGL at least version 3.3
    * Vulkan in planning
* MacOS:
    * Metal (recommend to use Metal since OpenGL is deprecated by Apple)
    * OpenGL at least version 3.3
* Android:
    * OpenGL ES at least version 3.0
    * Vulkan in planning
* iOS (11.0 +):
    * Metal (recommend to use Metal since OpenGLES is deprecated by Apple)
    * OpenGL ES at least version 3.0

## Sync Dependencies

These three-party dependencies are divided into three parts:
* The core dependencies which are required to build the core library. These dependencies are located in [deps/](../hab/DEPS).
* Other dependencies which are only required when developing Skity (e.g. testing, example, etc.). These dependencies are located in [deps/dev/](../hab/DEPS.dev).
* CI dependencies which are only required when running CI. These dependencies are located in [deps/ci/](../hab/DEPS.ci).


As described above.

If only need to build the core library, you can sync the core dependencies by running the following command in the root directory of the project:
```bash
# Linux & MacOS
./tools/hab sync # sync core dependencies

# Windows
./tools/hab.ps1 sync
```

For developers, you can sync the dev dependencies by running the following command:
```bash
# Linux & MacOS
./tools/hab sync --target dev # sync dev dependencies

# Windows
./tools/hab.ps1 sync --target dev
```

## Build Skity

Use CMake to build the project.

```
cmake -B <build_dir> <options>
cmake --build <build_dir>
```

Replace `<build_dir>` with the path to the build directory you want to create.

For example:
```
cmake -B build
cmake --build build
```

There are several options you can use to configure the build:

* Options for building Skity itself:
    * `SKITY_HW_RENDERER` - Option for GPU raster. Default is ON. Pass `-DSKITY_HW_RENDERER=OFF` in command line to disable it. (It is recommended not to turn this option off)
    * `SKITY_SW_RENDERER` - Option for CPU raster. Default is ON. Pass `-DSKITY_SW_RENDERER=OFF` in command line to disable it if does not needs the CPU raster pipeline.
    * `SKITY_GL_BACKEND` - Option for OpenGL backend. Default is ON. Pass `-DSKITY_GL_BACKEND=OFF` in command line to disable it. (OpenGL/GLES is almost supported by all platforms, so it is recommended not to turn this option off.)
    * `SKITY_MTL_BACKEND` - Option for Metal backend. Not available on Windows and Linux but open by default on Apple platform.
    * `SKITY_LOG` - Option for logging. Default is OFF since print log may slow down the performance. Pass `-DSKITY_LOG=ON` in command line to enable it.

* Options for build optional modules:
    * `SKITY_CODEC_MODULE` - Option for building codec module. Default is ON for some historical reasons. Pass `-DSKITY_CODEC_MODULE=OFF` in command line to disable it.

* Options for build other development components (Make sure you sync dev dependencies):
    * `SKITY_EXAMPLE` - Option for building example code. Default is OFF. Pass `-DSKITY_EXAMPLE=ON` in command line to enable it.
    * `SKITY_TEST` - Option for building test code. Default is OFF. Pass `-DSKITY_TEST=ON` in command line to enable it.

> Note: Other options not mentioned above are used internally and may be changed or removed in the future.

## Run Examples

Make sure sync the dev dependencies and open `SKITY_EXAMPLE` option during cmake config stage.

After build, you can find all example executable in `<build_dir>/example/case` directory.
Windows user can find the executable in `<build_dir>/<Debug/Release>/` directory. Based on the build type you choose.

You can run the example code like this:
```bash
# run the basic example
<build_dir>/example/case/basic_example <backend>
```

The available backend name for `<backend>` is:
* `gl` - OpenGL backend
* `metal` - Metal backend
* `software` - Software renderer backend


## Run Tests

Make sure sync the dev dependencies and open `SKITY_TEST` option during cmake config stage.

After build, you can find the  `<build_dir>/test/ut/skity_unit_test` executable. Windows user can find the executable in `<build_dir>/<Debug/Release>/skity_unit_test.exe` directory. Based on the build type you choose.

It can be run directly. For example:
```bash
# run the unit test
<build_dir>/test/ut/skity_unit_test
```

Or use `ctest` to run the test. For example:
```bash
# run the unit test
ctest --test-dir <build_dir>/test/ut
```

> Note: the golden test is only available on MacOS since it uses Metal backend.
> The detail of golden test can be found in [golden/README.md](../test/golden/README.md).
