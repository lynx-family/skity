# 2026 Roadmap

Currently, the Skity project is mainly used by internal projects within the [lynx-family](https://github.com/lynx-family). Therefore, the core planning is prioritized based on internal business requirements. If external developers or teams are interested in adopting Skity or have specific feature needs, please submit an issue in the GitHub repository to initiate discussions—community feedback will be an important reference for adjusting the roadmap.

> Note: This roadmap represents preliminary plans and concepts. It is not exhaustive, and there is no guarantee that all listed items will be completed within 2026. Priorities may be adjusted based on technical challenges, business needs, and community feedback.

## 1. Performance And Quality Optimization

The core focus of 2026 is to enhance rendering performance and visual quality across all supported platforms, with special attention to GPU-accelerated workflows.

### 1.1 Performance Enhancement


* **Extend Batch Drawing & Runtime Shaders**: Currently, Skity outperforms SKIA in simple geometric drawing scenarios, leveraging batch processing of drawing instructions and dynamically generated high-performance Shader code. In 2026, we plan to extend this approach to general rendering scenarios (e.g., complex vector graphics, text rendering, and image compositing).

* **Leverage Modern GPU APIs**: Explore the use of compute shaders and indirect drawing to offload CPU workloads, reduce draw call overhead, and improve parallel processing capabilities. This optimization will be prioritized for Metal and WebGPU backends initially, with gradual adaptation to other backends.

* **Profiling & Bottleneck Resolution**: Establish a comprehensive performance profiling system to identify and resolve bottlenecks in shader execution, memory bandwidth, and CPU-GPU synchronization. Target metrics include frame rate stability, draw call count reduction, and memory footprint optimization.

### 1.2 Rendering Quality Improvement



* **Advanced Anti-Aliasing (AA) Mechanism**: Address the memory overhead issue of MSAA (Multi-Sample Anti-Aliasing) on resource-constrained devices (e.g., older mobile phones using OpenGLES). The existing contour AA method based on alpha interpolation will be refined—optimizations include algorithm adjustment for better edge smoothness, performance tuning to reduce GPU latency, and compatibility improvements across different backends.


## 2. GPU Backends Expansion & Improvement

Skity aims to build a truly cross-platform hardware-accelerated rendering engine. While current support for OpenGL/ES, Metal, and WebGPU covers most scenarios, 2026 will focus on backend optimization and expansion to address existing limitations.

### 2.1 Vulkan Backend Support

* **Priority Development**: Introduce Vulkan as a new backend to address the memory and performance drawbacks of OpenGLES on mobile devices. Vulkan’s low-level control will enable better resource management, multi-threaded command recording, and improved power efficiency—critical for modern mobile and desktop applications.

* **Feature Parity & Compatibility**: Ensure the Vulkan backend supports all core Skity features (e.g., vector rendering, text rendering, image compositing) with feature parity to existing backends. Compatibility will be tested across major GPUs (ARM, NVIDIA, AMD) and operating systems (Android, Linux, Windows).

## 3. Stability & Compatibility

Ensure Skity’s reliability across diverse hardware and software environments through rigorous testing and compatibility improvements.

### 3.1 Testing & Quality Assurance


* **Cross-Platform Testing**: Expand the test suite to cover more device models, operating system versions, and GPU configurations—with a focus on edge cases (e.g., low-end mobile devices, older browsers).

* **Automated Testing Pipeline**: Implement continuous integration (CI) and continuous delivery (CD) pipelines to run performance, quality, and compatibility tests automatically for every code change.

* **Bug Fixing & Regression Prevention**: Prioritize fixing critical bugs reported by internal teams and the community, and establish safeguards to prevent regressions in core functionality.

### 3.2 Compatibility Improvements


* **GPU Driver Compatibility**: Address driver-specific issues by working around known GPU driver bugs and ensuring compatibility with both proprietary and open-source GPU drivers.

## 4. Version Release Plan

In 2026, Skity will release two major stable versions (1.0.x and 1.1.x) to deliver incremental value, with clear focus areas and quality guarantees for each release.

### 4.1 Version 1.0.x (H1 2026)


* **Release Goal**: Mark the first production-ready stable version of Skity, focusing on **core stability, performance baseline, and existing backend maturity**.

* **Key Deliverables**:

1. Finalize and stabilize core rendering features (vector graphics, text rendering, image compositing) across OpenGL/ES, Metal, and WebGPU backends.

2. Complete initial performance optimizations: batch drawing extension to complex vector scenarios, basic compute shader integration for Metal/WebGPU.

3. Refine the advanced AA mechanism (alpha interpolation-based contour AA) to resolve memory overhead issues on OpenGLES devices.

4. Launch the revamped official documentation and basic getting-started tutorials.

5. Implement critical bug fixes and compatibility improvements based on internal testing feedback.

* **Update Cadence**: 1-2 minor updates (e.g., 1.0.1, 1.0.2) post-launch, focusing on bug fixes, security patches, and minor compatibility tweaks.

### 4.2 Version 1.1.x (Feature-Driven Release)

* **Release Trigger**: The version will be officially released once **key flagship features (including Vulkan backend) reach production-ready maturity** — no fixed timeline, with release readiness determined by feature completeness, stability validation, and ecosystem compatibility.

