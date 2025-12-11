# RRectGeometry Instancing Batching Implementation

## Overview
This document describes the implementation of instancing batching for `WGSLRRectGeometry`, enabling multiple rounded rectangles with the same transform to be rendered in a single draw call.

## Changes Made

### 1. WGSLRRectGeometry Class (`wgsl_rrect_geometry.hpp`)

**Added Members:**
```cpp
struct InstanceData {
  RRect rrect;
  Paint paint;
};

std::vector<InstanceData> instance_data_;
```

**Added Methods:**
```cpp
bool CanMerge(const HWWGSLGeometry* other) const override;
void Merge(const HWWGSLGeometry* other) override;
```

**Removed Members:**
- `const RRect& rrect_` (replaced by vector storage)
- `const Paint& paint_` (replaced by vector storage)

### 2. WGSLRRectGeometry Implementation (`wgsl_rrect_geometry.cc`)

**Constructor Changes:**
- Now stores the first RRect and Paint in `instance_data_` vector
- Removed direct member initialization of `rrect_` and `paint_`

**New CanMerge() Method:**
- Checks if shader names match
- Verifies type compatibility via dynamic_cast
- Ensures paint styles (fill vs stroke) are compatible
- Returns true if geometries can be batched together

**New Merge() Method:**
- Accumulates instance data from other geometry
- Appends all instances from the merged geometry to the instance vector

**Updated PrepareCMD() Method:**
- Creates a vector of `Instance` structs from `instance_data_`
- Uses the same transform matrix for all instances (required by architecture)
- Uploads all instances in a single buffer
- Sets `cmd->instance_count` to the actual number of instances

### 3. HWDynamicRRectDraw Class (`hw_dynamic_rrect_draw.hpp`)

**Added Members:**
```cpp
HWWGSLGeometry* geometry_ = nullptr;
```

**Added Methods:**
```cpp
bool OnMergeIfPossible(HWDraw* draw) override;
```

### 4. HWDynamicRRectDraw Implementation (`hw_dynamic_rrect_draw.cc`)

**Updated OnGenerateDrawStep():**
- Stores the geometry pointer in `geometry_` member
- Changed from local variable to member to enable access in merge logic

**New OnMergeIfPossible() Method:**
- Delegates to parent class for initial checks
- Validates that both geometries exist
- Calls geometry's `CanMerge()` to check compatibility
- Calls geometry's `Merge()` to combine instances
- Returns true if merge was successful

## How It Works

### Batching Flow

1. **Creation**: Each RRect draw creates a `WGSLRRectGeometry` with one instance
2. **Merge Check**: When `HWDraw::MergeIfPossible()` is called:
   - Base class checks transform, clip, and scissor match
   - `OnMergeIfPossible()` checks geometry compatibility
   - `CanMerge()` verifies shader and paint style compatibility
3. **Merge**: If compatible, `Merge()` appends instance data
4. **Render**: `PrepareCMD()` uploads all instances and sets `instance_count`

### Batching Constraints

Due to the architecture, batching only works when:
- **Same transform**: Required by `HWDraw::MergeIfPossible()` (line 123 in hw_draw.hpp)
- **Same clip state**: Required by base merge check
- **Same scissor box**: Required by base merge check
- **Same paint style**: Fill or stroke must match
- **Same shader**: Automatically satisfied for same geometry type

### Key Design Decision: Shared Transform

The current implementation uses a **single transform matrix** for all batched instances because:

1. The `HWDraw::MergeIfPossible()` check requires identical transforms
2. The transform is passed to `PrepareCMD()` as a parameter, not per-instance
3. The `CommonSlot` uniform contains the transform, which is shared across all instances

**Note**: While the instance buffer includes space for a per-instance transform matrix (Vec4 m), it's currently used for the inverse jacobian for anti-aliasing calculations, not for different transforms.

To support different transforms per instance would require:
- Removing the transform equality check in base merge
- Storing per-instance transforms in `InstanceData`
- Updating shader to use per-instance transforms for positioning

## Performance Impact

### Before Batching
- 100 RRects with same transform = 100 draw calls
- Each draw: 384 bytes (vertices) + 288 bytes (indices) + 48 bytes (instance)
- Total: 72,000 bytes + 100 draw calls

### After Batching
- 100 RRects with same transform = 1 draw call
- One draw: 384 bytes (vertices) + 288 bytes (indices) + 4,800 bytes (instances)
- Total: 5,472 bytes + 1 draw call
- **Savings**: 93% less geometry data, 99% fewer draw calls

## Testing

To test the batching functionality:

1. **Create multiple RRects** with the same transform
2. **Verify merging**: Check that `OnMergeIfPossible()` returns true
3. **Check instance count**: Verify `cmd->instance_count` matches expected value
4. **Visual validation**: Ensure all RRects render correctly
5. **Performance profiling**: Measure draw call reduction

## Limitations

1. **Same transform required**: Cannot batch RRects with different transforms
2. **Same style required**: Fill and stroke RRects cannot batch together
3. **No fragment compatibility**: Different colors/gradients may prevent batching (depends on fragment implementation)

## Future Enhancements

1. **Per-instance transforms**: Modify architecture to support different transforms
2. **Advanced paint compatibility**: More sophisticated checks for mergeable paints
3. **Fragment batching**: Batch RRects with compatible but not identical paint properties
4. **Performance metrics**: Add instrumentation to track batching effectiveness

## References

- `src/render/hw/draw/geometry/wgsl_rrect_geometry.hpp`
- `src/render/hw/draw/geometry/wgsl_rrect_geometry.cc`
- `src/render/hw/draw/hw_dynamic_rrect_draw.hpp`
- `src/render/hw/draw/hw_dynamic_rrect_draw.cc`
- `src/render/hw/draw/geometry/wgsl_text_geometry.cc` (reference implementation)
- `docs/rrect_instancing_analysis.md` (original analysis)
