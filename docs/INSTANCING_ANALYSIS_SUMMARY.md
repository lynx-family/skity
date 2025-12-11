# RRectGeometry Instancing Support Analysis - Summary

## Question (问题)
阅读RRectGeometry的代码，分析是否能够支持instancing合批
(Read the RRectGeometry code and analyze whether it can support instancing batching)

## Quick Answer (快速回答)

**YES (是) - RRectGeometry CAN support instancing batching, but it is NOT currently implemented.**

**是的 - RRectGeometry 能够支持实例化合批，但目前尚未实现。**

---

## Technical Evidence (技术证据)

### ✅ Architecture Ready for Instancing (架构已支持实例化)

1. **Instanced Vertex Buffer Layout**
   - Uses `GPUVertexStepMode::kInstance` for per-instance attributes
   - Instance buffer contains 12 floats per RRect (48 bytes)
   
2. **Instance Data Structure**
   ```cpp
   struct Instance {
     Vec4 rect;      // Rectangle bounds
     Vec2 radii;     // Corner radii  
     Vec2 stroke;    // Stroke width and join type
     Vec4 m;         // Transform matrix
   };
   ```

3. **Shared Geometry**
   - Static vertex buffer (24 vertices) shared across all instances
   - Static index buffer (72 indices) shared across all instances

4. **Instance-Aware Shaders**
   - Vertex shader reads instance attributes at locations 1-4
   - Fragment shader handles per-instance variations

### ❌ What's Missing (缺少的部分)

1. **No Batching Logic**
   - `CanMerge()` not implemented (defaults to returning `false`)
   - `Merge()` not implemented (defaults to empty)

2. **Single Instance Only**
   - `cmd->instance_count = 1` (hardcoded)
   - Only one `Instance` struct created per draw call

3. **No Instance Accumulation**
   - No storage for multiple instances
   - No logic to collect compatible RRects

---

## Performance Impact (性能影响)

### Current (当前)
- 100 RRects = 100 draw calls
- 100 × (384 bytes vertices + 288 bytes indices + 48 bytes instance) = 72,000 bytes

### With Batching (合批后)  
- 100 RRects = 1 draw call
- 1 × (384 bytes vertices + 288 bytes indices) + 100 × (48 bytes instance) = 5,472 bytes
- **93% memory reduction for geometry data**
- **99% reduction in draw calls**

---

## Implementation Effort (实现工作量)

**LOW TO MODERATE (低到中等)** - The infrastructure is already in place:

1. Add `std::vector<Instance> instances_` to store multiple instances
2. Implement `CanMerge()` to check shader/paint compatibility
3. Implement `Merge()` to accumulate instances
4. Update `PrepareCMD()` to upload all instances and set correct `instance_count`
5. Update `HWDynamicRRectDraw` to attempt merging

Reference implementation available in `WGSLTextGeometry`.

---

## Recommendation (建议)

**IMPLEMENT INSTANCING BATCHING** for significant performance gains in applications that render multiple rounded rectangles (UI elements, cards, buttons, etc.).

**建议实现实例化合批**，以在渲染多个圆角矩形（UI元素、卡片、按钮等）的应用程序中获得显著的性能提升。

---

## Full Analysis Documents (完整分析文档)

- English: [docs/rrect_instancing_analysis.md](./rrect_instancing_analysis.md)
- 中文: [docs/rrect_instancing_analysis_zh.md](./rrect_instancing_analysis_zh.md)
