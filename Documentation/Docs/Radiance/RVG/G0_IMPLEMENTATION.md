# G0 Implementation Details

## Previous Architecture
Meshes were directly queried via pointers in `RenderItem` and ECS. No standardized indexing or handle-based generation checks existed.

## New Architecture
- **Geometry Handles**: Introduced `GeometryHandle` and `VirtualGeometryHandle` with generation and index tracking.
- **Capability Tiers**: Introduced `CapabilityTracker` to detect physical device extensions, limits, and subgroup details, outputting a detailed report at startup.
- **Routing rules**: Integrated checks to display routing per mesh.

## Files Created
- `Rendering/Geometry/GeometryHandle.h`
- `Rendering/Geometry/GeometryTypes.h`
- `Rendering/Geometry/CapabilityTiers.h`
- `Rendering/Geometry/CapabilityTiers.cpp`
- `Runtime/Public/GeometryHandleTests.h`
- `Runtime/Private/GeometryHandleTests.cpp`
- `Docs/Radiance/RVG/RVG_STATUS.md`
- `Docs/Radiance/RVG/RADIANCE_GEOMETRY_CONTRACT.md`
- `Docs/Radiance/RVG/RVG_ARCHITECTURE.md`
- `Docs/Radiance/RVG/RVG_CAPABILITY_TIERS.md`
- `Docs/Radiance/RVG/RVG_FALLBACK_POLICY.md`
- `Docs/Radiance/RVG/RVG_RESOURCE_OWNERSHIP.md`
