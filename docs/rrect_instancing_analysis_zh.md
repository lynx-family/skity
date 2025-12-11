# RRectGeometry 实例化合批分析报告

## 概述
本文档分析了 `WGSLRRectGeometry` 类，以确定它是否能够支持实例化合批（instancing batching），这将允许在单次绘制调用中渲染多个圆角矩形几何体。

## 当前实现分析

### 1. 现有的实例化支持
`WGSLRRectGeometry` 类**已经具备基本的实例化支持**：

#### 顶点缓冲区布局（第29-73行）
```cpp
std::vector<GPUVertexBufferLayout> layout = {
    // 顶点缓冲区
    GPUVertexBufferLayout{
        4 * sizeof(float),
        GPUVertexStepMode::kVertex,  // 每顶点数据
        ...
    },
    // 实例缓冲区
    GPUVertexBufferLayout{
        12 * sizeof(float),
        GPUVertexStepMode::kInstance,  // 每实例数据
        ...
    },
};
```

该布局包含两个缓冲区：
- **顶点缓冲区**：包含共享的顶点数据（24个顶点定义RRect形状）
- **实例缓冲区**：包含每个实例的数据（矩形边界、圆角半径、描边宽度、变换矩阵）

#### 实例数据结构（第75-83行）
```cpp
struct Instance {
  Vec4 rect;      // 矩形边界（left, top, right, bottom）
  Vec2 radii;     // 圆角半径
  Vec2 stroke;    // 描边宽度和连接类型
  Vec4 m;         // 变换矩阵组件
};
static_assert(sizeof(Instance) == 48);  // 12个浮点数
```

#### 当前使用方式（第358-364行）
```cpp
Instance instance = {
    Vec4{rect.Left(), rect.Top(), rect.Right(), rect.Bottom()},
    rrect_.GetSimpleRadii(), 
    Vec2{stroke_radius, join}, 
    m
};

cmd->instance_count = 1;  // 当前仅渲染1个实例
cmd->instance_buffer = context->stageBuffer->Push(&instance, sizeof(Instance));
```

**关键观察**：代码设置 `instance_count = 1`，意味着当前每次绘制调用只渲染一个RRect。

### 2. 与文本几何体合批的对比

`WGSLTextGeometry` 类展示了不同的合批方法：

#### CanMerge/Merge 模式（wgsl_text_geometry.cc 第84-92行）
```cpp
bool WGSLTextGeometry::CanMerge(const HWWGSLGeometry* other) const {
  return GetShaderName() == other->GetShaderName();
}

void WGSLTextGeometry::Merge(const HWWGSLGeometry* other) {
  auto o = static_cast<const WGSLTextGeometry*>(other);
  for (auto&& glyph_rect : o->glyph_rects_) {
    glyph_rects_.emplace_back(std::move(glyph_rect));
  }
}
```

文本几何体通过以下方式批处理多个字形：
1. 合并来自多个文本绘制的顶点数据
2. 共享相同的着色器和uniform绑定
3. 使用单次绘制调用处理连接的顶点/索引缓冲区

### 3. 架构差异

| 方面 | RRectGeometry | TextGeometry |
|------|---------------|--------------|
| **合批方法** | 实例化（相同几何体，多个实例） | 顶点合并（不同几何体组合） |
| **共享数据** | 顶点缓冲区（24个顶点）+ 索引缓冲区 | 无（每个字形有独特的顶点） |
| **每项数据** | 实例缓冲区（每个RRect 48字节） | 顶点缓冲区（每个字形不同） |
| **CanMerge/Merge** | 未实现 | 已实现 |
| **当前合批** | 否（instance_count=1） | 是（合并字形） |

## 分析：RRectGeometry 能否支持实例化合批？

### 答案：可以，但目前未实现

`WGSLRRectGeometry` 类具有实例化合批的**架构基础**：

#### 已具备的能力：
1. ✅ **支持实例化的顶点缓冲区布局**，使用 `GPUVertexStepMode::kInstance`
2. ✅ **高效的Instance数据结构**（每个RRect 48字节）
3. ✅ **静态顶点/索引缓冲区**，可在所有RRect实例间共享
4. ✅ **着色器代码**从实例属性读取（locations 1-4）

#### 缺失的部分：
1. ❌ **无 CanMerge() 实现** - 默认返回false
2. ❌ **无 Merge() 实现** - 默认为空
3. ❌ **每次绘制调用单个实例** - 硬编码 `instance_count = 1`
4. ❌ **无实例数据累积** - 仅存储一个 `Instance` 结构体

## 为什么实例化合批对RRect有益

### 内存效率
- **无合批**：N个RRect = N次绘制调用 × (24个顶点 + 72个索引 + 1个实例)
- **有合批**：N个RRect = 1次绘制调用 × (24个顶点 + 72个索引 + N个实例)
- **节省**：顶点/索引数据共享，每增加一个RRect仅需48字节

### 性能优势
1. **更少的绘制调用**：大幅减少CPU开销
2. **更少的状态改变**：减少管线和绑定组切换
3. **更好的GPU利用率**：GPU并行处理多个实例
4. **缓存友好**：相同的着色器、uniforms和顶点缓冲区

### 典型使用场景
在渲染包含许多圆角矩形的UI时（按钮、卡片、面板），实例化合批将显著提升性能：
- 100个RRect：1次绘制调用而非100次
- 共享384字节顶点缓冲区 + 288字节索引缓冲区
- 仅需4,800字节实例数据（相比重复的顶点数据）

## 实现复杂度

实现相对**简单直接**，因为：

1. **着色器已支持实例化**：正确读取实例属性
2. **数据结构已存在**：`Instance` 结构体定义完善
3. **模式可参考**：可遵循 `WGSLTextGeometry` 的 `CanMerge`/`Merge` 方法
4. **基础设施就绪**：渲染系统支持实例化

### 需要的关键改动：

1. **添加实例存储**到 `WGSLRRectGeometry`：
   ```cpp
   private:
     std::vector<Instance> instances_;
   ```

2. **实现 CanMerge()**：
   ```cpp
   bool CanMerge(const HWWGSLGeometry* other) const override {
     // 如果着色器相同且paint设置兼容则可合并
     return GetShaderName() == other->GetShaderName();
   }
   ```

3. **实现 Merge()**：
   ```cpp
   void Merge(const HWWGSLGeometry* other) override {
     auto o = static_cast<const WGSLRRectGeometry*>(other);
     instances_.push_back(o->instances_[0]);
   }
   ```

4. **更新 PrepareCMD()** 以上传所有实例：
   ```cpp
   cmd->instance_count = instances_.size();
   cmd->instance_buffer = context->stageBuffer->Push(
       instances_.data(), 
       instances_.size() * sizeof(Instance)
   );
   ```

5. **更新 HWDynamicRRectDraw** 以尝试与其他RRect绘制合并

## 潜在挑战

### 1. Paint兼容性
具有不同paint设置（颜色、混合模式、样式）的RRect不能批处理在一起，因为它们需要不同的片段着色器或uniforms。

**解决方案**：在 `CanMerge()` 中检查paint兼容性。

### 2. 变换矩阵处理
每个实例在实例数据中有自己的变换矩阵，因此已经支持不同的变换。

### 3. 裁剪深度
批次中的所有实例必须具有相同的裁剪深度值（它是uniform，而非每实例）。

**解决方案**：只合并具有相同裁剪深度的RRect。

### 4. 模板/遮罩操作
如果RRect需要不同的模板操作，则不能批处理。

**解决方案**：在 `CanMerge()` 中检查模板状态。

## 结论

**`WGSLRRectGeometry` 类具有出色的实例化合批架构支持，但目前未实现该功能。**

该类在设计时就考虑了实例化：
- 使用 `GPUVertexStepMode::kInstance`
- 定义了合适的 `Instance` 数据结构
- 着色器正确读取实例属性

然而，实际的合批逻辑（CanMerge/Merge）未实现，且 `instance_count` 硬编码为1。

**建议**：参照 `WGSLTextGeometry` 中使用的模式实现合批支持。这将为渲染大量圆角矩形的应用程序提供显著的性能改进，且实现复杂度相对较低。

## 参考文件

- `src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp`
- `src/render/hw/draw/geometry/wgsl_rrect_geometry.cc`
- `src/render/hw/draw/geometry/wgsl_text_geometry.hpp`
- `src/render/hw/draw/geometry/wgsl_text_geometry.cc`
- `src/render/hw/draw/hw_wgsl_geometry.hpp`
