# RRectGeometry 实例化合批实现

## 概述
本文档描述了 `WGSLRRectGeometry` 的实例化合批实现，使得具有相同变换的多个圆角矩形可以在单次绘制调用中渲染。

## 所做的修改

### 1. WGSLRRectGeometry 类 (`wgsl_rrect_geometry.hpp`)

**新增成员：**
```cpp
struct InstanceData {
  RRect rrect;
  Paint paint;
};

std::vector<InstanceData> instance_data_;
```

**新增方法：**
```cpp
bool CanMerge(const HWWGSLGeometry* other) const override;
void Merge(const HWWGSLGeometry* other) override;
```

**移除成员：**
- `const RRect& rrect_` (替换为向量存储)
- `const Paint& paint_` (替换为向量存储)

### 2. WGSLRRectGeometry 实现 (`wgsl_rrect_geometry.cc`)

**构造函数变更：**
- 现在将第一个 RRect 和 Paint 存储在 `instance_data_` 向量中
- 移除了 `rrect_` 和 `paint_` 的直接成员初始化

**新增 CanMerge() 方法：**
- 检查着色器名称是否匹配
- 通过 dynamic_cast 验证类型兼容性
- 确保 paint 样式（填充 vs 描边）兼容
- 如果几何体可以批处理在一起则返回 true

**新增 Merge() 方法：**
- 从其他几何体累积实例数据
- 将合并几何体的所有实例追加到实例向量中

**更新 PrepareCMD() 方法：**
- 从 `instance_data_` 创建 `Instance` 结构体向量
- 对所有实例使用相同的变换矩阵（架构要求）
- 在单个缓冲区中上传所有实例
- 设置 `cmd->instance_count` 为实际实例数量

### 3. HWDynamicRRectDraw 类 (`hw_dynamic_rrect_draw.hpp`)

**新增成员：**
```cpp
HWWGSLGeometry* geometry_ = nullptr;
```

**新增方法：**
```cpp
bool OnMergeIfPossible(HWDraw* draw) override;
```

### 4. HWDynamicRRectDraw 实现 (`hw_dynamic_rrect_draw.cc`)

**更新 OnGenerateDrawStep()：**
- 将几何体指针存储在 `geometry_` 成员中
- 从局部变量改为成员变量以便在合并逻辑中访问

**新增 OnMergeIfPossible() 方法：**
- 委托给父类进行初始检查
- 验证两个几何体都存在
- 调用几何体的 `CanMerge()` 检查兼容性
- 调用几何体的 `Merge()` 组合实例
- 如果合并成功返回 true

## 工作原理

### 合批流程

1. **创建**：每个 RRect 绘制创建一个带有一个实例的 `WGSLRRectGeometry`
2. **合并检查**：当调用 `HWDraw::MergeIfPossible()` 时：
   - 基类检查变换、裁剪和裁剪框是否匹配
   - `OnMergeIfPossible()` 检查几何体兼容性
   - `CanMerge()` 验证着色器和 paint 样式兼容性
3. **合并**：如果兼容，`Merge()` 追加实例数据
4. **渲染**：`PrepareCMD()` 上传所有实例并设置 `instance_count`

### 合批约束

由于架构限制，合批仅在以下情况下有效：
- **相同变换**：`HWDraw::MergeIfPossible()` 要求（hw_draw.hpp 第 123 行）
- **相同裁剪状态**：基础合并检查要求
- **相同裁剪框**：基础合并检查要求
- **相同 paint 样式**：填充或描边必须匹配
- **相同着色器**：对于相同几何体类型自动满足

### 关键设计决策：共享变换

当前实现对所有批处理实例使用**单个变换矩阵**，因为：

1. `HWDraw::MergeIfPossible()` 检查要求相同的变换
2. 变换作为参数传递给 `PrepareCMD()`，而非每个实例
3. `CommonSlot` uniform 包含变换，在所有实例间共享

**注意**：虽然实例缓冲区包含每实例变换矩阵的空间（Vec4 m），它当前用于抗锯齿计算的逆雅可比矩阵，而非不同的变换。

要支持每实例不同的变换需要：
- 移除基础合并中的变换相等检查
- 在 `InstanceData` 中存储每实例变换
- 更新着色器以使用每实例变换进行定位

## 性能影响

### 合批前
- 100 个相同变换的 RRect = 100 次绘制调用
- 每次绘制：384 字节（顶点）+ 288 字节（索引）+ 48 字节（实例）
- 总计：72,000 字节 + 100 次绘制调用

### 合批后
- 100 个相同变换的 RRect = 1 次绘制调用
- 一次绘制：384 字节（顶点）+ 288 字节（索引）+ 4,800 字节（实例）
- 总计：5,472 字节 + 1 次绘制调用
- **节省**：几何数据减少 93%，绘制调用减少 99%

## 测试

要测试合批功能：

1. **创建多个 RRect**，使用相同的变换
2. **验证合并**：检查 `OnMergeIfPossible()` 返回 true
3. **检查实例计数**：验证 `cmd->instance_count` 匹配预期值
4. **视觉验证**：确保所有 RRect 正确渲染
5. **性能分析**：测量绘制调用减少

## 限制

1. **需要相同变换**：无法批处理具有不同变换的 RRect
2. **需要相同样式**：填充和描边 RRect 无法一起批处理
3. **无片段兼容性**：不同的颜色/渐变可能阻止批处理（取决于片段实现）

## 未来增强

1. **每实例变换**：修改架构以支持不同的变换
2. **高级 paint 兼容性**：更复杂的可合并 paint 检查
3. **片段合批**：批处理具有兼容但不完全相同的 paint 属性的 RRect
4. **性能指标**：添加仪表以跟踪合批效果

## 参考文献

- `src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp`
- `src/render/hw/draw/geometry/wgsl_rrect_geometry.cc`
- `src/render/hw/draw/hw_dynamic_rrect_draw.hpp`
- `src/render/hw/draw/hw_dynamic_rrect_draw.cc`
- `src/render/hw/draw/geometry/wgsl_text_geometry.cc` (参考实现)
- `docs/rrect_instancing_analysis.md` (原始分析)
