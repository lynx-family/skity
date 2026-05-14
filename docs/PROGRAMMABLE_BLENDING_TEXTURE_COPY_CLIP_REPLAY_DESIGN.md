## Programmable Blending Texture Copy Clip Replay Design

这份文档描述基于当前 `main` 实现的 clip replay 设计，不直接继承
`texture_copy_temp1` 的代码。目标是为 texture-copy fallback 补上 clip 语义，
同时显式避开 temp1 中已经暴露出的错误做法。

## 背景

当前 `HWLayer::AddDraw()` 在 advanced blending 走 `DstReadStrategy::kTextureCopy`
时会拆分 draw pass：

1. 前一 pass 结束并把 color 内容 copy 到临时 texture。
2. 开启新的 render pass。
3. 在新 pass 中执行 advanced draw，并从 copied dst texture 读取目标色。

这个流程已经能解决 dst read，但还没有解决 clip 继承问题。新 render pass 的
depth / stencil attachment 会重新开始，而 Skity 的 path clip 正是通过
stencil + depth 建立的。因此：

- rect clip 的 scissor bounds 还在；
- path clip 的 depth/stencil 结果会丢失；
- split 后的 advanced draw 可能绕过之前已经建立的 clip。

## temp1 给出的启发

`texture_copy_temp1` 分支里，`docs/ADVANCED_BLENDING_TEXTURE_COPY_CLIP_REPLAY.md`
和对应实现给了几个重要启发：

- 只需要对 path clip 做 replay；rect clip 继续由 scissor 表达。
- replay 必须只发生在 texture-copy split 后的新 pass 中，普通路径不应为此付费。
- MSAA 场景下顺序必须是 `emulated load -> clip replay -> advanced draw`。
- split 时需要冻结一个“截至当前时刻”的 replay 计划，而不是让后续新增 clip
  反向污染之前的 pass。

这些方向是对的，但 temp1 的具体实现不能直接沿用。

## temp1 中不能照搬的点

### 1. 不能把同一个 `HWDraw*` 直接复用到多个 pass

temp1 的方案是在 `HWLayerState` 中追加 `clip_history_`，split 后把历史 clip
draw 直接 append 到新 pass。这个想法在当前 `HWDraw` 生命周期模型下是不可靠的：

- `HWDraw::Prepare()` 受 `prepared_` 保护，只会真正执行一次。
- `HWDraw::GenerateCommand()` 受 `generated_` 保护，只会真正执行一次。

因此，同一个 `HWDraw*` 即使被放进多个 pass，也不会为每个 pass 重新生成独立命令。
这与 clip replay 的需求是冲突的。我们后续必须 replay “新的 draw 实例”或“新的命令”，
不能 replay “旧的 draw 指针”。

### 2. 不能把 clip replay 仅理解为 active clip 的回放

temp1 文档里有一个判断是值得保留的：仅 replay 当前 active clip 并不安全。
当前 clip depth 的形成依赖 save / restore 过程中产生过的 clip draw 顺序；
某些已经 restore 的 clip，虽然不再 active，但它们写入过的 depth/stencil 仍可能影响
后续 clip 语义。

因此第一版设计应优先保证语义正确，而不是急于把 replay 范围缩成“只 replay active clip”。

### 3. 不能把 temp1 的 golden 资源直接当正确性依据

temp1 提供了专门的 clip replay 文档、golden 命名和测试入口，这些测试基础设施方向值得借鉴。
但该分支最终渲染结果错误，且 clip replay case 本身也处于试验状态，所以：

- 可以借鉴它的测试组织方式；
- 不应该把 temp1 里的 expected image 当作正确 oracle；
- 应该基于当前 `main` 重新构造稳定、可解释的 clip replay case。

## 当前 `main` 的约束

当前主线实现有几个会直接影响设计的约束：

- `HWLayerState` 只维护当前 clip stack、clip bounds 和最后一个 clip draw，
  没有 replay plan。
- `pending_clip_` 只会 flush 到当前 pass。
- `HWRenderPassBuilder` 在新 pass 中会清空 depth / stencil。
- `HWDrawPass` 已经有 `emulated_load_info` 和
  `dst_read_texture_copy_info`，说明 pass split 模型已经存在，不需要重做。
- `HWDraw` 是 one-shot prepare / generate，不适合跨 pass 复用实例。

这意味着我们的设计应优先复用“当前的 pass split 框架”，而不是再造另一套 pass 模型。

## 设计目标

- 在 texture-copy fallback 中恢复 path clip 语义。
- 不改变 framebuffer-fetch 路径和普通 non-advanced draw 路径。
- 不要求 depth/stencil attachment 做 store/load。
- 不依赖复用旧的 `HWDraw*`。
- 先保证语义正确，再考虑缩小 replay 范围或做性能优化。

## 非目标

- 不在这一轮同时重构 MSAA emulated load 为新的 pass abstraction。
- 不在这一轮引入“只 replay active clip”的优化。
- 不直接修改 public API。
- 不把 texture-copy unsupported fallback 问题和 clip replay 一起解决。

## 提议方案

### 1. 引入可回放的 clip 记录，而不是 `clip_history_` 裸指针列表

为 path clip 增加内部 replay 记录，例如：

```cpp
struct ClipReplayRecord {
  HWDraw* source_draw = nullptr;
  uint32_t record_index = 0;
  int32_t parent_record_index = -1;
  Rect scissor_box = {};
};
```

这里的关键不是字段名，而是语义：

- `source_draw` 只作为源信息和最终 clip depth 的承载者，不直接被 replay。
- `parent_record_index` 表达这个 clip 在原始录制时依赖的上一层 clip。
- `scissor_box` 保留该 clip 创建时的 clip bounds。
- 记录顺序必须与原始 clip draw 出现顺序一致。

当前版本只对 path clip 建记录；rect clip 继续通过 `CurrentClipBounds()` 进入 draw 的
scissor，不加入 replay draw 列表。

### 2. 明确 `ClipRect` 的 replay 规则

这里需要把“rect clip 不加入 replay draw 列表”再说得更精确一些，避免后续实现时误把
所有 `ClipRect` 都当作“不需要 replay”。

当前主线里，`HWCanvas::OnClipRect()` 只有在下面条件同时成立时，才会走硬件快速路径：

- `ClipOp == kIntersect`；
- 当前 matrix 只有 `scale + translate`；
- clip 可以直接折叠进 `CurrentClipBounds()`。

在这条快速路径下：

- `ClipRect` 不会生成独立的 clip draw；
- `HWLayerState` 只会收窄 `clip_bounds`；
- 后续每个 draw 在 `HWLayer::AddDraw()` 里都会继承这个 `clip_bounds` 对应的 scissor。

因此，对这类“简单相交 `ClipRect`”，texture-copy split 后通常不需要单独 replay。只要
split 后的新 draw 继续带着相同的 scissor，约束就仍然存在。

但下面两类 `ClipRect` 不能套用这个结论：

- `ClipOp::kDifference`；
- 非 `scale + translate` 变换下的 `ClipRect`，例如旋转、斜切、透视。

这些情况在当前实现里会回退到基类 `Canvas::OnClipRect()`，再转成 path clip 流程处理。
一旦走到 `OnClipPath()`，它本质上就不再是“只靠 scissor 表达的 rect clip”，而是会生成
真正的 clip draw，并进入 path clip 的 replay 语义。

所以当前设计应明确采用下面这条规则：

- 简单相交 `ClipRect`：不加入 replay record，继续依赖 `clip_bounds -> scissor` 传播；
- fallback 成 path 的 `ClipRect`：按 path clip 处理，需要进入 replay record。

### 3. 为 clip draw 提供 replay clone 能力

必须为 clip replay 生成“新的 draw 实例”。推荐两种实现方向，优先第一种：

1. 给 clip draw 增加内部 clone 接口，例如 `MakeClipReplayClone()`。
2. 或者给 `ClipReplayRecord` 保存足够的构造参数，由 replay builder 重新 new 出
   等价的 clip draw。

不建议在第一版直接保存“命令对象副本”，因为现有 command 生成依赖 pipeline、buffer、
arena allocator 和 clip depth，生命周期耦合更重。

对于当前主要的 path clip，`HWDynamicPathClip` 已经保存了 replay 所需的大部分源信息：

- `transform`
- `path`
- `clip op`
- `bounds`
- `use_gpu_tessellation`

因此第一版完全可以只支持 `HWDynamicPathClip` replay，不急着把所有 clip draw 类型一网打尽。

### 4. 在 split 时冻结 replay 范围，在 prepare 时物化 replay draw

推荐把 replay 流程拆成两个阶段：

#### 录制阶段

当 `HWLayer::AddDraw()` 因 texture copy 拆 pass 时：

- 新 pass 记录一个 `clip_replay_until_record_index`。
- 这个 index 表示“新 pass 需要 replay 到哪一条 clip 记录为止”。
- split 之后新增的 clip 只影响之后的新 pass，不回写到已经存在的 replay plan。

这样可以避免 temp1 的隐式共享状态问题。

#### prepare 阶段

在 `state_.FlushClipDepth()` 之后，为每个需要 clip replay 的 pass 物化新的 replay draws：

- 按记录顺序遍历 `0..clip_replay_until_record_index`。
- 为每条记录创建新的 replay clip draw。
- 把源 draw 上最终 resolved 的 `clip_depth` 复制到 replay draw。
- 根据 `parent_record_index` 把 replay draws 之间的 `clip_draw` 指针重新串起来。
- 将这些 replay draws 放进 pass 的 `clip_replay_draws` 列表，而不是污染原始 `draw_ops`。

这里要注意：源 draw 在 `FlushClipDepth()` 之后才拿到最终 clip depth，因此 replay draw 的
创建时机不能早于 prepare。

### 5. draw pass 内部顺序必须固定

对需要 replay 的 split pass，执行顺序固定为：

1. `emulated load draw`，仅在 MSAA 场景存在。
2. `clip replay draws`。
3. 正常 draw ops，其中第一个通常就是 advanced draw。

原因如下：

- emulated load 要先恢复 color，否则会被 replay 后的 clip 限制。
- clip replay 只写 stencil/depth，不应影响 color。
- advanced draw 要在 clip 已恢复后执行，才能正确受 clip 约束。

这点与 temp1 的判断一致，应直接保留。

### 6. replay 的第一版采用“全历史到 split 点”策略

考虑到当前 clip depth 语义和 restore 行为较复杂，第一版建议明确采用：

- replay 从 layer 开始到 split 时刻为止的全部 path clip 记录；
- 不尝试只保留 active clip；
- 不尝试跨 split 做公共 replay 缓存。

这样做的缺点是成本为 `O(texture_copy_split_count * clip_count)`，但它把优化问题留到后面，
先把语义做对。等 golden 覆盖稳定后，再考虑：

- 只 replay active clip；
- 基于 save/restore epoch 做增量 replay；
- 对 replay plan 做剪枝。

## 需要新增的数据结构

推荐最小新增以下内部结构：

```cpp
struct ClipReplayRecord;

struct ClipReplayPlan {
  uint32_t replay_until_record_index = 0;
  std::vector<HWDraw*> replay_draws;
};
```

以及：

- `HWLayerState` 持有 `std::vector<ClipReplayRecord>`。
- `HWDrawPass` 可选持有 `ClipReplayPlan`。

这能把“录制期的逻辑记录”和“prepare 后的真实 replay draw 实例”分开，避免 temp1 那种
把记录和执行对象混在同一个 `clip_history_` 里的问题。

## 对 `HWLayerState` 的建议职责

`HWLayerState` 更适合承担“clip 语义记录”，而不是“直接生成 replay draw”。
建议职责如下：

- `SaveClipOp()` 时登记 path clip 的 replay record。
- 继续维护 `clip_stack_`、`CurrentClipBounds()`、`LastClipDraw()`。
- 暴露“截至当前时刻的 replay 截止位置”。
- 不负责创建新的 replay draw 实例。

对应地，`HWLayer` 更适合承担：

- split 时为新 pass 冻结 replay plan；
- prepare 时物化 replay draw；
- Draw 时按顺序执行 replay。

## 为什么不建议直接把 replay draw 插回 `draw_ops`

第一版更推荐在 `HWDrawPass` 中把 replay draw 放入独立字段，例如
`clip_replay_draws`，而不是直接 append 到 `draw_ops`。原因是：

- `draw_ops` 语义上是“用户录制出来的原始 draw 和内部 emulated load draw”；
- clip replay 是 split 之后派生出来的执行产物，更像 pass 内部的 restore step；
- 独立字段更方便后续做统计、debug 和验证；
- 更容易在 Draw 顺序中明确插入 `emulated load -> clip replay -> draw_ops`。

## 性能与复杂度判断

### 正确性优先级

当前 clip replay 是 correctness gap，不是微优化项。第一版应优先保证：

- split 后 clip 不丢；
- save / restore 语义正确；
- MSAA 与 texture copy 组合顺序正确。

### 成本预期

- CPU 侧会多出 replay plan 构建和 replay draw clone。
- GPU 侧会多出 replay clip draw 的 stencil/depth pass。
- 成本只发生在 texture-copy fallback 上，不影响 framebuffer fetch。

这是可以接受的第一版代价。

## 验证计划

实现前应先补测试，至少覆盖：

### 结构性测试

- split pass 时是否冻结了正确的 replay 截止位置。
- replay draw 是否是新实例，而不是原始 `HWDraw*`。
- replay draw 的 `clip_depth` 是否与源 draw 一致。
- replay draw 的 parent clip 链是否正确重建。
- MSAA split pass 中 replay 顺序是否位于 emulated load 之后。

### golden 测试

- root layer: `clip + advanced blend + texture copy`。
- sub layer / saveLayer: `clip + advanced blend + texture copy`。
- 两个连续 advanced blend，中间不新增 clip。
- `save / clip / restore / advanced blend` 组合。
- MSAA 版本单独保留一条最小 case。

测试组织方式可以借鉴 temp1：

- 同一 case 同时支持默认路径和强制 texture-copy 路径。
- golden 读取路径允许按 backend / dst-read strategy 选择 oracle。

但 golden 图应基于当前 `main` 重新生成，不复用 temp1 的结果文件。

## 分阶段实现建议

建议按以下顺序推进，而不是一次性落地：

1. 先补设计相关结构性测试壳子。
2. 引入 `ClipReplayRecord` 和 split 时的 replay plan 冻结。
3. 只支持 `HWDynamicPathClip` 的 replay clone。
4. 接入单采样 texture-copy clip replay。
5. 再接入 MSAA 场景。
6. 最后再考虑 replay 范围优化。

## 与当前 refactor notes 的关系

这份文档聚焦“怎么设计 clip replay”。
其余与 texture copy 相关但不直接属于 clip replay 的问题，例如：

- unsupported dst read 的 fail-fast 语义；
- emulated load 的两阶段提交；
- copy region / UV mapping builder；
- golden caps override 正式化；

继续在 `docs/PROGRAMMABLE_BLENDING_TEXTURE_COPY_REFACTOR_NOTES.md` 中跟踪。
