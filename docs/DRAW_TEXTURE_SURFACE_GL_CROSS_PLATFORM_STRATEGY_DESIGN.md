# DrawTextureSurfaceGL Cross-Platform Mode Design

## 背景

当前 `DrawTextureSurfaceGL` / `GLDrawTextureLayer` 本身并不是 Android 专属实现；
真正的 Android-only 约束主要在 `GPUContextImplGL::CreateSurface()` 的工厂分支里：

- `DrawTextureSurfaceGL` 的类定义和 `OnBeginNextFrame()` 逻辑是通用的；
- `CreateDrawTextureSurface()` 的声明和定义被 `#ifdef SKITY_ANDROID` 包住；
- 非 Android 平台在 `!has_stencil_attachment || sample_count > 1` 时会固定创建
  `BlitSurfaceGL`，无法自然进入 `DrawTextureSurfaceGL` 路径。

这使得当前代码把“平台差异”和“surface 呈现策略”绑在了一起：

- Android 默认走 draw-to-texture + final present；
- 非 Android 默认走 offscreen blit；
- 但这两者本质上都只是 GL root surface 的不同 presentation mode。

如果我们想在 mac 上测试 `GLDrawTextureLayer`，或者未来让其他平台也复用
`DrawTextureSurfaceGL`，继续依赖平台宏会越来越别扭。

## 目标

- 把 `DrawTextureSurfaceGL` 从 Android-only 入口改成全平台可选策略。
- 默认行为尽量保持不变，避免影响现有 Android / 非 Android 路径。
- 让 mac / 测试环境可以显式创建 `DrawTextureSurfaceGL`，无需伪造 Android 宏。
- 改动尽量收敛在 GL surface factory 层，不在这一轮重构 `GLDrawTextureLayer`
  内部实现。

## 非目标

- 不在这一轮同时重写 `DrawTextureSurfaceGL` 的 flush / present 细节。
- 不在这一轮改变 `DirectSurfaceGL`、`BlitSurfaceGL`、`TextureSurfaceGL`
  的职责边界。
- 不在这一轮处理 “所有 `GLRootLayer` 子类都支持 texture-copy dst-read”
  的能力扩展。
- 不顺带调整公共 C API / public header，除非现有测试入口确实无法表达该策略。

## 当前问题

当前 `GPUContextImplGL::CreateSurface()` 的关键逻辑可以概括为：

1. 纹理 surface 直接走 `TextureSurfaceGL`。
2. 如果 target 没有 stencil attachment，或者 sample count 大于 1：
   - Android：`CreateDrawTextureSurface(...)`
   - 其他平台：`CreateBlitSurface(...)`
3. 其他情况走 `CreateDirectSurface(...)`

这个分支的实际问题不是功能不工作，而是表达方式不对：

- “为什么选 draw-texture” 被编码成平台判断；
- “能不能在当前平台试验 draw-texture” 没有显式入口；
- 后续如果要做 golden / debug / caps override，就只能继续堆宏或平台特判。

## 最小方案

### 1. 把平台宏分支改成显式 mode 选择

为 GL root surface 引入一个内部策略枚举，例如：

```cpp
enum class GLSurfaceMode {
  kAuto,
  kDirect,
  kBlit,
  kDrawTexture,
};
```

其中：

- `kDirect` 对应 `DirectSurfaceGL`
- `kBlit` 对应 `BlitSurfaceGL`
- `kDrawTexture` 对应 `DrawTextureSurfaceGL`
- `kAuto` 代表“沿用当前默认行为”

第一版不需要把这个枚举公开成新的大范围 API；只要能通过
`GPUSurfaceDescriptorGL` 或测试专用入口把它传到 `CreateSurface()` 即可。

### 2. `kAuto` 保持现有默认行为

`kAuto` 的策略解析应尽量复用今天的默认语义：

- `GLSurfaceType::kTexture` 仍直接创建 `TextureSurfaceGL`
- 普通 root target 且满足 direct 条件时，仍优先 `DirectSurfaceGL`
- 当 `!has_stencil_attachment || sample_count > 1` 时：
  - Android 默认解析为 `kDrawTexture`
  - 非 Android 默认解析为 `kBlit`

也就是说，第一版只是把“平台判断”收敛进一个 mode resolver，而不是立刻修改
所有平台的默认选择。

这样做的好处是：

- 主线行为基本不变；
- 风险主要局限在 factory 选择层；
- 后续如果验证表明 mac 上 `kDrawTexture` 足够稳定，再单独讨论是否调整 `kAuto`
  的默认策略。

### 3. 显式允许非 Android 强制选择 `kDrawTexture`

一旦 mode 被显式指定为 `kDrawTexture`，`CreateSurface()` 就直接创建
`DrawTextureSurfaceGL`，不再受 `SKITY_ANDROID` 限制。

对应改动最小可以是：

- 去掉 `gpu_context_impl_gl.hpp/.cc` 中 `CreateDrawTextureSurface(...)`
  的 `#ifdef SKITY_ANDROID`
- 新增一个 helper，例如：

```cpp
GLSurfaceMode ResolveSurfaceMode(
    const GPUSurfaceDescriptorGL& desc);
```

- `CreateSurface()` 根据 resolved mode 分发到：
  - `CreateDirectSurface(...)`
  - `CreateBlitSurface(...)`
  - `CreateDrawTextureSurface(...)`

这样 `DrawTextureSurfaceGL` 的可用性就变成“factory policy 是否允许创建”，
而不是“当前编译平台是不是 Android”。

### 4. 失败策略采用“显式 override，保守 fallback”

第一版建议区分两类场景：

- `kAuto`
  - 如果未来发现某个平台 / 驱动不适合 `kDrawTexture`，resolver 继续返回
    `kBlit` 即可；
  - 这条路径以兼容性优先。
- 显式指定 `kDrawTexture`
  - 如果创建所需的基础条件不满足，优先打印 debug log / DCHECK；
  - release 下可以保守 fallback 到 `kBlit`，避免直接中断整个 surface 创建。

这样测试环境可以强制覆盖 `DrawTextureSurfaceGL` 路径，而生产默认路径仍保留
足够保守的回退空间。

## 建议落点

### 1. 描述层

优先在 `GPUSurfaceDescriptorGL` 增加一个可选字段，例如：

```cpp
GLSurfaceMode surface_mode = GLSurfaceMode::kAuto;
```

原因是它最贴近“本次创建这个 GL surface 想怎么 present”的语义，也最方便测试环境
做显式覆盖。

如果短期不想改 descriptor，也可以先在 `GPUContextImplGL` 增加一个仅测试使用的
内部 override 开关；但这只是过渡方案，长期可维护性不如把策略挂在 descriptor 上。

### 2. 工厂层

把当前散在 `CreateSurface()` 里的 if/else 收敛成：

```cpp
auto mode = ResolveSurfaceMode(*gl_desc);

switch (mode) {
  case GLSurfaceMode::kDirect:
    return CreateDirectSurface(...);
  case GLSurfaceMode::kBlit:
    return CreateBlitSurface(...);
  case GLSurfaceMode::kDrawTexture:
    return CreateDrawTextureSurface(...);
  default:
    ...
}
```

这样后续若要新增 debug trace、策略统计、backend-specific override，都有一个单点。

### 3. 实现层

`DrawTextureSurfaceGL` 和 `GLDrawTextureLayer` 第一版不需要大改：

- `DrawTextureSurfaceGL::OnBeginNextFrame()` 继续创建 `GLDrawTextureLayer`
- `BlitSurfaceGL` 继续保留现有实现，作为默认兼容路径和 fallback
- Android 的现有行为通过 `kAuto -> kDrawTexture` 保持

## 为什么这是最小改动

这个方案刻意不碰下面这些更大的问题：

- `GLDrawTextureLayer` 是否立刻支持 texture-copy dst-read
- `GLPartialDrawTextureLayer` 是否也要一起接入
- `DrawTextureSurfaceGL` 是否应成为所有平台的默认策略
- 是否要把 resolver 再下沉成更正式的 GPU caps / device policy

它只做一件事：把 “Android-only 宏分支” 改成 “全平台可选策略分支”。

对当前代码组织来说，这是最小且最自然的第一步。

## 验证建议

第一阶段只需要验证“可创建”和“行为不漂移”：

- Android 默认路径仍走 `kAuto -> kDrawTexture`
- mac 默认路径仍走 `kAuto -> kBlit`
- mac 在显式指定 `kDrawTexture` 后，能够成功创建 `DrawTextureSurfaceGL`
- 基础 golden 至少覆盖一组强制 `kDrawTexture` 的 GL case

如果这一步稳定，再继续推进：

- 为 golden / test env 提供正式 mode override 入口
- 评估 `GLDrawTextureLayer` 接入 `SupportsTextureCopyDstRead()`
- 再讨论是否要把非 Android 的 `kAuto` 默认值从 `kBlit` 调整为 `kDrawTexture`

## 后续演进

这份草案之后，比较自然的第二步有两条：

1. 测试导向
   - 先给 GL golden test 增加 mode override，确保 mac 能覆盖
     `DrawTextureSurfaceGL` 路径。
2. 能力导向
   - 在 `GLDrawTextureLayer` 上补 `SupportsTextureCopyDstRead()` /
     `OnCopyToDstTexture()` / `GetResolveColorTexture()`，让它不仅能被创建，
     还真正参与 texture-copy dst-read。

建议顺序是先做第 1 条，再做第 2 条。先把“能稳定创建并测试”解决，再扩能力，
风险会更可控。
