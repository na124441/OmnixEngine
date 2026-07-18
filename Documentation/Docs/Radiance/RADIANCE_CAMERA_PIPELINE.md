# Radiance Camera and Color Pipeline Specification

This document specifies the design, order of operations, and math used in the Omnix Engine Radiance Color and Camera Pipeline.

---

## 1. Pipeline Overview

All shading in Omnix Engine is computed in **Linear Rec. 709 (sRGB primaries)** working space. Tone mapping, exposure adjustment, color grading, and final gamma correction are performed in a single one-way pass at the end of the frame rendering pipeline.

The processing order of operations is defined as follows:

```
[Composed Linear HDR Scene Radiance]
                |
                v
      [Exposure Adjustment] (Auto-Adapted or Manual)
                |
                v
      [Tone Mapping Curve]  (Reinhard, ACES, or Filmic)
                |
                v
      [Color Grading]       (White Balance -> Contrast -> Saturation -> Lift/Gamma/Gain)
                |
                v
      [Gamma Correction]    (2.2 power curve)
                |
                v
       [sRGB Display LDR]
```

---

## 2. Operations & Mathematics

### A. Exposure Adjustment
Calculates the multiplier applied to linear HDR radiance:
- **Manual**: $C_{exposed} = C_{linear} \cdot E_{manual}$
- **Automatic (GPU Adapted)**: $C_{exposed} = C_{linear} \cdot E_{adapted}$
  Where $E_{adapted}$ is read from the GPU `ExposureBuffer` storage buffer, computed continuously over time.

### B. Tone Mapping Operators
Transforms high-dynamic range values into displayable $[0, 1]$ bounds:
1. **Reinhard**:
   $$f(x) = \frac{x}{x + 1}$$
2. **ACES (Approximation)**:
   $$f(x) = \frac{x \cdot (2.51 \cdot x + 0.03)}{x \cdot (2.43 \cdot x + 0.59) + 0.14}$$
3. **Filmic (Uncharted 2 Curve)**:
   $$f(x) = \frac{W \cdot (A \cdot x + B \cdot C) + D \cdot E}{x \cdot (A \cdot x + B) + D \cdot F} - \frac{E}{F}$$
   (With standard exposure shoulder/toe coefficients).

### C. Color Grading
Performed in linear sRGB space:
1. **White Balance (Temperature & Tint)**:
   Adjusts red and blue channels based on temperature, and green/magenta based on tint to simulate light source shifts.
2. **Contrast**:
   $$C_{contrast} = (C - 0.5) \cdot \text{Contrast} + 0.5$$
3. **Saturation**:
   $$C_{sat} = \text{mix}(\text{Luminance}(C), C, \text{Saturation})$$
   Where $\text{Luminance}(C) = 0.2126 \cdot R + 0.7152 \cdot G + 0.0722 \cdot B$.
4. **Lift, Gamma, Gain**:
   - **Lift (Offset)**: Shifts shadows. $C_{lift} = C \cdot (1 - L) + L$
   - **Gamma (Power)**: Shifts midtones. $C_{gamma} = C_{lift}^{1/\text{Gamma}}$
   - **Gain (Slope)**: Shifts highlights. $C_{gain} = C_{gamma} \cdot \text{Gain}$

### D. Gamma Correction
Prepares LDR output for the monitor display transfer function:
$$C_{final} = C_{gain}^{1/2.2}$$
swapchain outputs are assumed to be standard sRGB format, so double gamma correction is strictly prohibited.
