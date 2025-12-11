# RRectGeometry Instancing Batching Analysis

## Overview
This document analyzes the `WGSLRRectGeometry` class to determine whether it can support instancing batching, which would allow multiple RRect geometries to be rendered in a single draw call.

## Current Implementation Analysis

### 1. Existing Instancing Support
The `WGSLRRectGeometry` class **already has basic instancing support**:

**Evidence from `wgsl_rrect_geometry.cc`:**

#### Vertex Buffer Layout (Lines 29-73)
```cpp
std::vector<GPUVertexBufferLayout> layout = {
    // vertex buffer
    GPUVertexBufferLayout{
        4 * sizeof(float),
        GPUVertexStepMode::kVertex,  // Per-vertex data
        ...
    },
    // instance buffer
    GPUVertexBufferLayout{
        12 * sizeof(float),
        GPUVertexStepMode::kInstance,  // Per-instance data
        ...
    },
};
```

The layout includes two buffers:
- **Vertex buffer**: Contains shared vertex data (24 vertices defining the RRect shape)
- **Instance buffer**: Contains per-instance data (rect bounds, radii, stroke width, transform matrix)

#### Instance Data Structure (Lines 75-83)
```cpp
struct Instance {
  Vec4 rect;      // Rectangle bounds (left, top, right, bottom)
  Vec2 radii;     // Corner radii
  Vec2 stroke;    // Stroke width and join type
  Vec4 m;         // Transform matrix components
};
static_assert(sizeof(Instance) == 48);  // 12 floats
```

#### Current Usage (Lines 358-364)
```cpp
Instance instance = {
    Vec4{rect.Left(), rect.Top(), rect.Right(), rect.Bottom()},
    rrect_.GetSimpleRadii(), 
    Vec2{stroke_radius, join}, 
    m
};

cmd->instance_count = 1;  // Currently renders only 1 instance
cmd->instance_buffer = context->stageBuffer->Push(&instance, sizeof(Instance));
```

**Key Observation**: The code sets `instance_count = 1`, meaning it currently only renders one RRect per draw call.

### 2. Comparison with Text Geometry Batching

The `WGSLTextGeometry` class demonstrates a different batching approach:

#### CanMerge/Merge Pattern (Lines 84-92 in wgsl_text_geometry.cc)
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

Text geometry batches multiple glyphs by:
1. Merging vertex data from multiple text draws
2. Sharing the same shader and uniform bindings
3. Using a single draw call with concatenated vertex/index buffers

### 3. Architecture Differences

| Aspect | RRectGeometry | TextGeometry |
|--------|---------------|--------------|
| **Batching Approach** | Instancing (same geometry, multiple instances) | Vertex merging (different geometries combined) |
| **Shared Data** | Vertex buffer (24 vertices) + Index buffer | None (each glyph has unique vertices) |
| **Per-Item Data** | Instance buffer (48 bytes per RRect) | Vertex buffer (varies per glyph) |
| **CanMerge/Merge** | Not implemented | Implemented |
| **Current Batching** | No (instance_count=1) | Yes (merges glyphs) |

## Analysis: Can RRectGeometry Support Instancing Batching?

### Answer: YES, but not currently implemented

The `WGSLRRectGeometry` class has the **architectural foundation** for instancing batching:

#### What's Already in Place:
1. ✅ **Instancing-ready vertex buffer layout** with `GPUVertexStepMode::kInstance`
2. ✅ **Efficient Instance data structure** (48 bytes per RRect)
3. ✅ **Static vertex/index buffers** that can be shared across all RRect instances
4. ✅ **Shader code** that reads from instance attributes (locations 1-4)

#### What's Missing:
1. ❌ **No CanMerge() implementation** - returns false by default
2. ❌ **No Merge() implementation** - empty by default  
3. ❌ **Single instance per draw call** - hardcoded `instance_count = 1`
4. ❌ **No instance data accumulation** - only stores one `Instance` struct

## Why Instancing Batching is Beneficial for RRect

### Memory Efficiency
- **Without batching**: N RRects = N draw calls × (24 vertices + 72 indices + 1 instance)
- **With batching**: N RRects = 1 draw call × (24 vertices + 72 indices + N instances)
- **Savings**: Vertex/index data shared, only 48 bytes per additional RRect

### Performance Benefits
1. **Fewer draw calls**: Major CPU overhead reduction
2. **Less state changes**: Fewer pipeline and binding group switches
3. **Better GPU utilization**: GPU processes multiple instances in parallel
4. **Cache friendly**: Same shader, uniforms, and vertex buffers

### Typical Use Case
When rendering UI with many rounded rectangles (buttons, cards, panels), instancing batching would dramatically improve performance:
- 100 RRects: 1 draw call instead of 100
- Shared 384-byte vertex buffer + 288-byte index buffer
- Only 4,800 bytes of instance data (vs. repeating vertex data)

## Implementation Complexity

The implementation would be **relatively straightforward** because:

1. **Shader is already instance-aware**: It reads instance attributes correctly
2. **Data structure exists**: The `Instance` struct is well-defined
3. **Pattern exists**: Can follow `WGSLTextGeometry`'s `CanMerge`/`Merge` approach
4. **Infrastructure ready**: The rendering system supports instancing

### Key Changes Needed:

1. **Add instance storage** to `WGSLRRectGeometry`:
   ```cpp
   private:
     std::vector<Instance> instances_;
   ```

2. **Implement CanMerge()**:
   ```cpp
   bool CanMerge(const HWWGSLGeometry* other) const override {
     // Can merge if same shader and compatible paint settings
     return GetShaderName() == other->GetShaderName();
   }
   ```

3. **Implement Merge()**:
   ```cpp
   void Merge(const HWWGSLGeometry* other) override {
     auto o = static_cast<const WGSLRRectGeometry*>(other);
     instances_.push_back(o->instances_[0]);
   }
   ```

4. **Update PrepareCMD()** to upload all instances:
   ```cpp
   cmd->instance_count = instances_.size();
   cmd->instance_buffer = context->stageBuffer->Push(
       instances_.data(), 
       instances_.size() * sizeof(Instance)
   );
   ```

5. **Update HWDynamicRRectDraw** to attempt merging with other RRect draws

## Potential Challenges

### 1. Paint Compatibility
RRects with different paint settings (color, blend mode, style) cannot be batched together because they need different fragment shaders or uniforms.

**Solution**: Check paint compatibility in `CanMerge()`.

### 2. Transform Matrix Handling
Each instance has its own transform matrix in the instance data, so different transforms are already supported.

### 3. Clip Depth
All instances in a batch must have the same clip depth value (it's a uniform, not per-instance).

**Solution**: Only merge RRects with identical clip depths.

### 4. Stencil/Mask Operations
If RRects require different stencil operations, they cannot be batched.

**Solution**: Check stencil state in `CanMerge()`.

## Conclusion

**The `WGSLRRectGeometry` class has excellent architectural support for instancing batching but does not currently implement it.**

The class was designed with instancing in mind:
- Uses `GPUVertexStepMode::kInstance` 
- Defines a proper `Instance` data structure
- Shaders correctly read instance attributes

However, the actual batching logic (CanMerge/Merge) is not implemented, and `instance_count` is hardcoded to 1.

**Recommendation**: Implement batching support following the pattern used in `WGSLTextGeometry`. This would provide significant performance improvements for applications that render many rounded rectangles, with relatively low implementation complexity.

## References

- `src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp`
- `src/render/hw/draw/geometry/wgsl_rrect_geometry.cc`
- `src/render/hw/draw/geometry/wgsl_text_geometry.hpp`
- `src/render/hw/draw/geometry/wgsl_text_geometry.cc`
- `src/render/hw/draw/hw_wgsl_geometry.hpp`
