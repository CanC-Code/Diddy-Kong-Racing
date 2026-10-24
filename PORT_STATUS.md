# DKR aarch64 Android Port — Status

## Working (proven)
- Full ROM boot, extraction, init chain
- Display-list walker (70K+ TEXRECTs/frame)
- Software RDP rasterizer (correct geometry)
- Framebuffer readback → MJPEG stream → Chrome
- F3DEX1 quick opcodes (B2/B3/E4) + custom Rare 0x07 (SetTextureImage)
- PI DMA — 110 calls, ROM reads from correct offsets, writes to correct host addresses
- RDRAM mapping — 12MB at host base, valid framebuffers at 0x400000/0x440000
- Chrome stream stable across 450+ frames

## Root-caused (this session)
- Textures are DMA'd to **heap buffers** (0x79b5..., 0x7ae1..., 0x79fb...)
- RDP reads texture from **RDRAM offset** (0x2FE8C0) which is zero
- ROM at that offset HAS data — so copy exists but bridge is missing
- **Remaining bug:** heap → RDRAM mirror step in texture loader

## Next investigation
1. Find DKR's texture-load function in decomp source (`src/`)
2. Locate where it places DMA'd data into RDRAM for RDP consumption
3. Verify that code path runs on this port
4. If missing: implement heap→RDRAM bridge in either game code or `ResourceMgr_HandleDma`

## Downstream (B-K-A)
Same texture bridge fix applies to Banjo-Kazooie (same Rare engine).
