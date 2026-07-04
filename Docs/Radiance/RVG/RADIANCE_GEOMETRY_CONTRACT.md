# Radiance Geometry Contract

This document defines the permanent geometry contracts for conventional and virtual geometry in Omnix.

## Geometry Handles
All meshes are referenced by lightweight handle structures:
- `GeometryHandle`: Tracks index and generation for conventional meshes.
- `VirtualGeometryHandle`: Tracks index and generation for virtualized geometry meshes.

## Generation Validation & Staleness
1. Handles contain a `generation` ID.
2. The central `GeometryArena` or runtime registry tracks slot residency and current active generations.
3. If an asset is reloaded or deleted:
   - The slot is marked inactive or the generation ID is incremented.
   - Any stale handle referencing the old generation will fail validation and fall back.
