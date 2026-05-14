# Programmable Blending Texture Copy Refactor Notes

这份文档记录当前 programmable blending / texture copy 实现中值得后续重构的点。当前目标不是否定现有移植结果：golden 已经覆盖了 framebuffer fetch 与 texture copy 两条链路，并包含 GL / Metal、sample count 1 / 4 的基础验证。这里主要整理代码坏味道、隐含约束和潜在风险，方便后续逐项收敛。

## 当前链路概览

涉及的核心代码主要分布在：

- `src/render/hw/dst_read_strategy.*`：根据 blend mode 和 GPU caps 统一选择 `DstReadStrategy`。
- `src/render/hw/hw_canvas.cc`：创建 layer，并通过 helper 设置 GL / Metal 对应的 layer RT origin。
- `src/render/hw/hw_layer.cc`：在 `AddDraw` 中为 texture copy 拆分 draw pass；通过 builder 计算 copy region 和 UV mapping；在 `Draw` 中完成 copy；在 MSAA 场景下插入模拟 load 的 draw。
- `src/render/hw/layer/hw_sub_layer.cc`：sub layer 的 layer back draw 也会根据 caps 选择 framebuffer fetch 或 texture copy。
- `src/render/hw/draw/wgx_programmable_blending.*`：生成 programmable blending shader 代码，并在 texture copy 路径绑定 dst texture / sampler / UV mapping。
- `src/render/hw/draw/hw_wgsl_shader_writer.cc`：在 texture copy 路径通过 fragment position 计算 dst UV 并执行 blending。
- `src/render/hw/hw_draw_pass.hpp`：用 `DstTextureCopyInfo` 承载 texture copy pass 的依赖信息，仍保留部分公开 pass 状态。
- `src/gpu/gpu_blit_pass.*`：通过结构化 `TextureCopyRegion` 执行 texture-to-texture copy。

## 推进状态

### 已完成

- Dst read strategy 选择已收敛到 `ResolveDstReadStrategy(BlendMode, const GPUCaps&)`，`HWCanvas` 和 `HWSubLayer` 不再各自手写判断。
- GL layer RT origin 选择已从 `HWCanvas::GenLayer` 的裸判断收进本文件内 helper，当前仍保留现有 backend 语义。
- Texture copy pass 的前后依赖已从 `prev_draw_pass` / 多个散字段收敛为 `DstTextureCopyInfo` 指针传递。
- Copy region / UV mapping 已抽成 `BuildDstTextureCopyInfo()` 和 `BuildDstUVMapping()`，集中处理 empty、clamp 和 RT origin。
- `CopyTextureToTexture` 已改成结构体 `GPUBlitPass::TextureCopyRegion`，避免长参数调用点混淆 src / dst offset 和 width / height。
- Root layer / sub layer 已增加显式 `SupportsTextureCopyDstRead()` 能力查询，dst read strategy 会在 texture copy unsupported 时避免选择 texture copy。
- MSAA emulated load 的状态已从单独的 `resolve_image_for_load` 字段收敛为 `EmulatedLoadInfo`，显式表达 load draw 和 deferred resolve image 的绑定关系。
- Programmable blending 的 texture-copy shader layout 已收敛成局部常量，shader 生成和 bind group 设置不再各自手写 group / binding / uniform 类型字符串。

### 未完成

- MSAA emulated load 仍是插入普通 draw 的模型，后续还可以继续提升为更明确的 pass load/restore 行为。
- Clip replay 还没有纳入 texture copy split 后的新 pass。
- Golden caps override 仍依赖测试侧修改 caps，缺少正式 override 入口。
- Texture copy 的结构性测试和 debug counter 还没有补齐。

## 高优先级风险

### 1. Dst read strategy 选择逻辑分散（已完成）

`HWCanvas::SetupDstReadStrategyForDraw` 和 `HWSubLayer::OnPrepare` 已改为调用 `ResolveDstReadStrategy(BlendMode, const GPUCaps&)`。后续如果要扩展强制 texture copy、backend-specific fallback、debug override 或 layer 特例，优先扩展这个 resolver，而不是在调用点重新分叉。

### 2. GL RT origin 逻辑放在 `HWCanvas::GenLayer`（部分完成）

当前 GL / non-GL 的选择已经收进 `HWCanvas` 文件内的 `ResolveLayerRTOrigin()` helper，去掉了直接散在 `GenLayer` 中的 backend 判断。这个逻辑本身可能是正确方向，但它仍位于 canvas layer 创建流程里，backend coordinate convention 和 texture copy UV mapping 的关系还不够显式。

建议后续明确 `LayerRTOrigin` 的语义和来源：它应该是 render target / backend 的属性，还是 texture copy dst sampling 的属性。然后把 GL / Metal 的 origin 决策下沉到更靠近 render target 创建或 backend adapter 的位置。

### 3. Texture copy pass 拆分依赖隐式状态（部分完成）

`HWLayer::AddDraw` 遇到 `kTextureCopy` 时仍会操作 `draw_passes_.back()`，并 push 一个新的 draw pass。但 texture copy 的资源和映射信息已经收敛进 `DstTextureCopyInfo`，后续 shader bind group 也不再通过 `context->prev_draw_pass` 反查，而是读取显式的 `dst_read_texture_copy_info`。

这条链路有几个隐含约束：

- texture copy draw 必须紧跟在一个已经准备好 dst texture 的 previous pass 后面。
- `dst_read_texture_copy_info` 必须带有有效的 `texture`、`sampler`、`uv_mapping`。
- pass split、copy、shader binding 三者必须保持严格顺序。

这些约束现在还没有被类型系统或集中校验完整表达出来，更多依赖调用顺序。建议后续把 “dst read attachment” 或 “texture copy dependency” 做成一等概念，让 pass split、copy 和 shader binding 的顺序关系更清楚。

### 4. Copy region 和 UV mapping 缺少集中校验（已完成基础收敛）

texture copy 路径已经通过 `BuildDstTextureCopyInfo()` 和 `BuildDstUVMapping()` 集中计算 copy region 与 UV mapping。当前已经处理：

- draw bounds 为空或宽高为 0 时，不创建 texture copy draw。
- bounds 为负数或超出 layer 时，先 clamp 到 layer bounds，再转成 `GPURegion`。
- `LayerRTOrigin::kTopLeft` / `kBottomLeft` 的 UV 偏移在同一个 helper 中处理。

仍待确认的问题是：copy region 是否应该考虑 AA 膨胀、clip / scissor 后的最终设备范围，以及是否需要为这个 builder 补小粒度结构性测试。

### 5. MSAA 模拟 load 还是临时方案形态

sample count 大于 1 时，当前实现会创建一个模拟 load 的 draw，把上一 pass resolve 出来的 texture 画回当前 pass。当前代码注释已把这块收敛为一个明确后续方向：用 pass-level load / restore abstraction 替换 draw-based emulated load。

这块的风险主要在于它把 render pass load 行为伪装成普通 draw，导致 blend、scissor、sample count、color format、resolve image 生命周期等状态都需要额外同步。未来如果 backend 支持真实 load / store / resolve 控制，这里应该有更明确的 pass-level abstraction，而不是临时插入一个 draw。

## 中优先级坏味道

### 1. `HWDrawPass` 是公开字段状态包（部分完成）

`HWDrawPass` 现在承载了 draw ops、texture copy info、texture copy read dependency、load emulation image 等状态。texture copy 相关字段已经收敛到 `DstTextureCopyInfo`，但 pass 本身仍是公开字段状态包，字段之间的组合约束还没有由类型或接口完整表达。

建议后续把 load emulation 相关字段也收敛成单独结构体或 pass capability，并继续减少 “某个字段非空时另外几个资源必须有效” 这种散落约定。

### 2. Shader bind group 使用硬编码 binding（已完成基础收敛）

`WGXProgrammableBlending::GenSourceWGSL` 中 texture copy 资源固定使用：

- `@group(2) @binding(0)`：UV mapping uniform。
- `@group(2) @binding(1)`：sampler。
- `@group(2) @binding(2)`：dst texture。

`SetupBindGroup` 里也依赖相同约定，并且还通过 uniform 类型字符串判断 `"vec4<f32>"`。这会让 shader layout 演进时比较脆弱。当前已先把 group / binding / uniform 类型字符串集中到 `WGXProgrammableBlending` 文件内常量，保证 shader 生成和 bind group 设置读取同一份 layout 约定。

后续如果 layout 继续变复杂，可以把这些常量升级成 layout descriptor，并进一步避免运行时用字符串判断 uniform 类型。

### 3. `SupportsFramebufferFetch()` 命名容易误导（已完成）

`WGXProgrammableBlending::SupportsFramebufferFetch()` 当前实际表达的是当前 shader 使用 framebuffer fetch 路径，而不是 GPU 是否支持 framebuffer fetch。这个命名容易和 caps 里的 `supports_framebuffer_fetch` 混淆。

当前已改成 `UsesFramebufferFetch()`，用于表达 shader 当前选择的是 framebuffer fetch dst read 路径。

### 4. 失败路径多数是静默返回

`WGXProgrammableBlending::SetupBindGroup` 中多处失败会直接 return，例如找不到 uniform、类型不匹配、无法创建 bind group 等。作为原型这能让渲染流程继续走，但后续调试 texture copy 问题时会很难定位。

建议至少在 debug 构建里加 `DEBUG_CHECK` 或日志，把这些失败变成可见信号。

### 5. Shader writer 中保留了调试注释代码

texture copy shader 里还保留了注释掉的 UV debug 逻辑。建议后续清理，或者改成正式的 debug shader 开关，避免临时调试代码长期留在主路径中。

## 测试与工具链风险

### 1. Golden 强制 caps 目前通过 `const_cast`

golden test 为了强制 framebuffer fetch / texture copy，会修改 GPU caps 中的 `supports_framebuffer_fetch`。这解决了验证需求，但从测试基础设施角度看还是偏 hack。

建议后续为 test env 或 GPUDevice 增加正式的 caps override 入口，让测试路径不用破坏 caps 的 const 语义。

### 2. Golden 覆盖了视觉结果，但缺少结构性测试

当前 golden 已覆盖 GL / Metal、framebuffer fetch / texture copy、sample count 1 / 4，这是非常重要的端到端验证。但 texture copy 的一些边界更适合补充较小的结构性测试：

- strategy selection 是否符合 caps 和 override。
- copy region 对负坐标、空 bounds、越界 bounds 的处理。
- RT origin 到 UV mapping 的转换。
- MSAA 下是否创建 load emulation draw。
- texture copy pass split 后显式 dst read 依赖是否满足。

这些测试不一定要马上加，但后续重构前最好先补一部分，避免清理过程中改坏隐含行为。

### 3. 性能风险暂时没有观测

texture copy 路径会打断 draw pass 合并，并在每个需要 dst read 的 pass 后执行 copy。当前 golden 主要验证正确性，没有验证 copy 次数、copy region 大小和 pass 数量。

建议后续加入 debug counter 或 trace 信息，至少能在本地看到每帧 texture copy 次数、总 copy 像素数、pass split 次数。

## `texture_copy_temp1` 可参考内容

`texture_copy_temp1` 是一次更激进的移植和重构尝试，最终绘制结果不正确，所以不应该直接照搬。但这个分支里有几类设计整理值得保留，用来指导后续小步重构。

### 1. 分阶段提交和风险分层

temp1 的文档把工作拆成 GPU blit copy primitive、shader-side texture-copy programmable blending、layer pass splitting、MSAA/coordinate hardening、golden coverage 几个阶段，并给每个阶段标了风险等级。这个拆分很值得继续采用。

当前更稳妥的原则是：每次只处理一个风险源，并在每一步后跑对应 golden。不要再把 pass split、坐标翻转、MSAA load、clip replay 和测试结构同时改掉。

### 2. `CreateDstCopyInfo` / `BuildDstCopyInfo` 的方向是对的（已完成基础收敛）

temp1 中把 copy rect、backend copy region 和 UV mapping 收敛到 `CreateDstCopyInfo()`，这个方向已经在主线中落为 `BuildDstTextureCopyInfo()` / `BuildDstUVMapping()`。当前代码已经把 copy rect、backend copy region、UV mapping 和 texture/sampler 资源统一放进 `DstTextureCopyInfo`。

当前 builder 职责包括：

- 对 draw bounds 做 floor / ceil。
- clamp 到 layer bounds。
- 处理 empty rect，不创建 0 尺寸 texture。
- 生成 backend copy region。
- 生成 shader 采样用的 UV mapping。
- 统一处理 `LayerRTOrigin::kTopLeft` / `kBottomLeft`。

这个 builder 后续应该补小粒度测试，尤其覆盖 fractional bounds、负坐标、越界 bounds、top-left origin 和 bottom-left origin。temp1 里已经暴露过：坐标逻辑如果散在 GL copy 和 shader mapping 两边，非常容易漏翻转或双重翻转。

### 3. Copy API 使用结构体比长参数更可维护（已完成）

temp1 中曾设计过 `GPUTextureCopyRegion`，包含 `src_x`、`src_y`、`dst_x`、`dst_y`、`width`、`height`。当前主线已将 `CopyTextureToTexture` 改成 `GPUBlitPass::TextureCopyRegion` 参数，调用点不再依赖长参数顺序。

后续如果要继续加强这块，可以在 backend copy API 中补 bounds debug assert，或者增加从 “source region copy 到 dst origin” 的小 helper，减少调用点重复初始化 `dst_x = 0` / `dst_y = 0`。

### 4. Root layer / sub layer 的 copy 能力应该显式表达（已完成基础收敛）

temp1 中有 `SupportsTextureCopyDstRead()` 这个方向：不是所有 layer 都天然支持 texture-copy dst read。特别是 GL direct FBO root 这类没有可采样 / 可 copy texture 的路径，应该显式标记 unsupported，而不是走到后面才绑定空 texture 或静默失败。

当前主线已经给 `HWLayer` 增加 `SupportsTextureCopyDstRead()`，默认返回 false；`MTLRootLayer`、`GLExternTextureLayer` 和 `HWSubLayer` 这些具备可 copy dst source 的 layer 返回 true。`HWCanvas::SetupDstReadStrategyForDraw()` 会把当前 layer 的能力传给 resolver；`HWSubLayer` 的 layer back draw 复用自身已经选择好的 dst read strategy，不再单独根据 caps 重新判断。

后续仍可以把这个能力进一步下沉到 render target / attachment 层，让它不只由 layer 类型决定。

### 5. MSAA load emulation 需要单独建模（部分完成）

temp1 把 `resolve_image_for_load` 和 `resolve_load_texture` 作为一组独立资源处理，并拆出类似 `PrepareEmulatedLoadResources()` 的阶段。虽然当时整体结果不正确，但这个方向比把 load draw 临时塞在 pass split 中更容易维护。

当前主线已经把 MSAA emulated load 从单独的 `resolve_image_for_load` 字段收敛成 `EmulatedLoadInfo`，其中包含 load draw 和 deferred resolve image。这样已经能更清楚地区分：

- advanced blending 需要读取的局部 dst copy。
- split pass 后为了恢复 color 内容而需要的整层 load snapshot。

这两者的生命周期、copy region、采样范围和性能成本都不同，不应该长期混在同一组字段或同一段流程里。后续仍可以继续把 emulated load 从普通 draw_ops 中提升为更明确的 pass-level load/restore 行为。

### 6. Clip replay 是后续必须认真处理的风险点

temp1 的 `ADVANCED_BLENDING_TEXTURE_COPY_CLIP_REPLAY.md` 指出一个重要问题：texture copy split 会开启新的 render pass，而新的 render pass 不继承上一段 pass 的 depth / stencil attachment 内容。Skity 的 GPU path clip 又依赖 depth / stencil，因此 texture copy 和 clip 组合时，可能需要在新 pass 中 replay clip draw。

temp1 的方案是记录 `clip_history_`，split 后把历史 clip draw 追加到新 pass，顺序是 emulated load、clip replay、advanced draw。这个思路值得作为后续专题研究，但不能直接照搬，原因是：

- replay 所有历史 clip 可能带来 O(split count * clip count) 的 GPU 成本。
- restore / difference clip / 嵌套 clip 的语义需要非常小心。
- replay 的 clip depth 分配必须和原 pass 等价。
- MSAA load draw 必须排在 clip replay 前，否则恢复 color 的 draw 会被 clip 限制。

建议后续新增 clip + texture copy golden 前，先补一份明确的 clip replay 设计和结构性测试。当前不应该在没有测试保护的情况下顺手加 clip replay。

### 7. 小 helper 能显著降低 `HWLayer` 复杂度（部分完成）

temp1 里有 `CurrentDrawPass()`、`AddDrawPass()`、`SplitDrawPassForTextureCopy()`、`PrepareDstCopyResources()` 这类 helper。虽然实现不能直接拿来用，但拆分方向是好的：`HWLayer::AddDraw()` 现在仍然承担了太多职责，包括普通 draw setup、clip flush、merge、copy region、pass split、MSAA load。

建议后续按行为边界逐步拆：

- 当前 pass 获取和创建。
- texture copy split。
- dst copy info 构建。（已完成基础收敛）
- dst copy resource prepare。
- load emulation resource prepare。

每拆一步都保持行为不变，并用当前 texture-copy golden 验证。

### 8. Shader writer 的语义 helper 值得保留（部分完成）

temp1 中 `HWWGSLShaderWriter` 使用了 `NeedsProgrammableBlending()`、`NeedsFramebufferFetch()`、`NeedsTextureCopy()` 这类 helper。这个方向能减少 shader writer 里散落的条件判断，也能避免把 caps support 和当前 shader path 混在一起。

当前主线已经把 `WGXProgrammableBlending::SupportsFramebufferFetch()` 改为 `UsesFramebufferFetch()`，避免和 GPU caps support 混淆。后续如果继续整理 shader writer，可以补 `NeedsTextureCopy()` 之类的 helper，让 framebuffer fetch / texture copy 的分支表达更一致。

### 9. temp1 的经验教训

temp1 最大的参考价值不是某一段代码，而是它说明了 texture copy fallback 的风险源必须被隔离。后续每次重构应遵守：

- 先保留当前 golden 通过的行为，再移动代码。
- 每次只改一个概念，不把坐标、MSAA、clip、backend copy 混在同一轮。
- 任何从 temp1 借回来的设计，都先写成小 helper 或内部结构，不直接扩大 public API。
- 对 temp1 中曾经导致结果不正确的区域，优先补测试，再改实现。

## 建议重构顺序

1. [done] 抽出 dst read strategy 选择逻辑，消除 `HWCanvas` 和 `HWSubLayer` 的重复判断。
2. [done] 把 texture copy 相关字段从 `HWDrawPass` 中收敛成结构体，并集中校验资源有效性。
3. [done] 抽出 copy region / UV mapping builder，统一处理 origin、clamp、empty bounds 和 scissor。
4. [done] 把 texture-to-texture copy API 改成结构体 region，减少参数顺序和坐标语义错误。
5. [done] 明确 root/sub layer 是否支持 texture-copy dst read，尤其处理 GL direct FBO unsupported 路径。
6. [done] 梳理 MSAA load emulation，把它从普通 draw 的临时模型逐步提升为明确的 pass 行为。
7. [next] 在有测试保护后，再研究 texture copy split 后的 clip replay。
8. [done] 整理 programmable blending shader binding layout，去掉散落的 binding 和字符串类型判断。
9. [todo] 给 golden caps override 增加正式测试入口，替换 `const_cast`。
10. [todo] 在上述节点补充小粒度测试或 debug counter，再继续做更大范围的接口重构。

## 待确认问题

- Texture copy 的 copy region 应该严格使用 draw bounds，还是需要考虑 AA 膨胀、clip、scissor 后的最终设备范围？
- GL 路径的 `LayerRTOrigin::kBottomLeft` 是否应该成为所有 GL render target 的统一属性？
- MSAA 场景下哪些 backend 可以依赖 render pass load，哪些 backend 必须用模拟 load？
- root layer 和 sub layer 的 texture copy 行为是否应该共享同一套 abstraction？
- framebuffer fetch 与 texture copy 是否应该在更高层暴露为显式渲染策略，方便测试和调试工具强制选择？
