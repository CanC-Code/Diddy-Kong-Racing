# DKR aarch64 Android Port — Status

## Architecture — PROVEN WORKING
- ROM extraction from compressed cartridge image
- Display list walker: 19K+ TEXRECTs/frame, no crashes
- Software RDRAM rasterizer: correct geometry, bounding boxes, per-frame fade
- Palette lookup: verified correct (palette bar renders real 16-color gradient)
- TMEM population: 1582-2231 bytes per texture frame
- Framebuffer readback → MJPEG → Chrome streaming: 450+ frame stability
- Camera matrices: POPMTX implemented (1284 pops/frame), no drift
- G_SETCOMBINE, G_SETOTHERMODE, G_SETPRIM, G_SETFILL all wired

## Remaining blocker — texture source
`g_embedded_lut` compiled against DKR v77, ROM is v80 (SHA1 verified).
Every texture offset the game requests maps to v77 positions in v80 ROM
→ asset_load reads garbage headers (isCompressed=205, uncompressedSize=204MB)
→ crash (mitigated by BKA-BADHDR safety check)

## Texture extraction path (chosen architecture)
Ship `tools/dkr_assets_tool` (ARM64 ELF, verified buildable) + configs in APK.
On first launch, extract PNGs from ROM to app-private storage.
`load_texture()` reads from PNG cache instead of ROM.
No decompressed assets shipped — on-device extraction, ROM only.

## Blockers on extractor
- `data/`, `asm/`, `extract-ver/` are generated (gitignored)
- `extract-ver/` restored from upstream/dkrMod — verified valid JSON
- `data/` still missing — tool crashes before logging (raw throw in cEnumsHelper)
- Suspected cause: missing C context (.ctx files) used to parse struct layouts

## Next session opening tasks (in order)
1. Run `./generate_ctx.sh` to regenerate `data/` (if this works, done)
2. Else check upstream branches for committed `data/` (dkrMod, benchmark-tool)
3. Else rebuild `dkr_assets_tool` from source with try/catch added around throw 1 in cEnumsHelper.cpp
4. Once extractor runs: write `texture_png_loader.cpp` + patch `load_texture()` (~120 lines)
5. Ship tool+configs in APK, run at first launch, load PNGs at runtime
6. RARE logo renders with real pixels

## Estimated remaining work
4-8 hours of focused tooling + PNG loader integration.
All renderer architecture is complete — this is a texture source pipeline task.
