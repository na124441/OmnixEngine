# RVG Fallback Policy

This document details the fallback states in case of RVG processing failures.

## Fallback States
1. **Lower Geometric Detail**: Use parent nodes in virtual geometry hierarchy.
2. **Resident Parent Representation**: Render parent page if child is not resident.
3. **Root Representation**: Fall back to the coarsest root representation.
4. **Conventional Fallback Mesh**: If virtual geometry cannot compile/load, fallback to conventional CPU/GPU indexed mesh.
5. **Visible Error Material**: Display a bright error grid if assets are missing.
