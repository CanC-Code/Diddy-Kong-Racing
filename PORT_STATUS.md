# DKR aarch64 Android Port — Status

## Working
- Full ROM boot, extraction, init chain
- Display-list walker (14,412 TEXRECTs/frame)
- Software RDP rasterizer (correct geometry)
- Framebuffer readback → MJPEG stream → Chrome
- All F3DEX1 opcode dispatch including B2/B3/E4 quick forms

## Known gaps
- Texture upload path missing (F0/F2/F3/F5/FD never fire in walker)
  → DKR uploads at boot, walker starts cold
- Color combiner is texel*prim only (no 2-cycle, fog, alpha)
- 3D geometry via vertices not yet wired
- Audio not started
- Input not wired

## Current visual
RARE logo renders correctly as geometry; rainbow fallback proves pipeline.
Real textures require boot-time DL instrumentation.
