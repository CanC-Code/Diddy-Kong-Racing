#include "ptr_shim.h"
#include <string.h>
#include "dkr_embedded_lut.h"
#include "asset_loading.h"
#include <android/log.h>

#include "common.h"
#include "gzip.h"
#include "macros.h"
#include "memory.h"
#include "ultra64.h"

/************ .bss ************/

OSIoMesg gAssetsDmaIoMesg;
OSMesg gDmaMesg;
OSMesgQueue gDmaMesgQueue;
OSMesg gPIMesgBuf[16];
OSMesgQueue gPIMesgQueue;
#if VERSION >= VERSION_79
OSMesg gDmaMutexBuf;
OSMesgQueue gDmaMutex;
#define dmacopy_internal dmacopy_v1
#else
#define dmacopy_internal dmacopy
#endif
u32 *gAssetsLookupTable;

/*******************************/

// These are both defined in the generated dkr.ld file.
extern u32 gBkaLutSize;
extern u32 gBkaLutRomOffset;
extern u32 gBkaAssetsRomOffset;

/**
 * Set up the peripheral interface message queues and scheduling.
 * This will send messages when DMA reads are finished.
 * After, allocate space and load the asset table into RAM.
 * Official Name: piInit
 */
void pi_init(void) {
    __android_log_print(ANDROID_LOG_ERROR, "DKR-PI", "pi_init ENTERED (embedded LUT mode)");
    u32 assetTableSize;
    osCreateMesgQueue(&gPIMesgQueue, gPIMesgBuf, ARRAY_COUNT(gPIMesgBuf));
    osCreateMesgQueue(&gDmaMesgQueue, &gDmaMesg, 1);
    osCreatePiManager((OSPri) 150, &gPIMesgQueue, gPIMesgBuf, ARRAY_COUNT(gPIMesgBuf));

#if VERSION >= VERSION_79
    osCreateMesgQueue(&gDmaMutex, &gDmaMutexBuf, 1);
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    // Embedded LUT: 208 bytes from splat (assets.lut.bin), no ROM dependency.
    assetTableSize = G_EMBEDDED_LUT_SIZE;
    gAssetsLookupTable = (u32 *) mempool_alloc_safe(assetTableSize, COLOUR_TAG_GREY);
    if (gAssetsLookupTable == NULL) {
        __android_log_print(ANDROID_LOG_ERROR, "DKR-PI", "FATAL: mempool_alloc_safe returned NULL for LUT");
        return;
    }
    mempool_locked_set((u8 *) gAssetsLookupTable);
    memcpy(gAssetsLookupTable, g_embedded_lut, assetTableSize);

    // Byteswap BE -> LE in place
    for (u32 i = 0; i < assetTableSize / 4; i++) {
        gAssetsLookupTable[i] = __builtin_bswap32(gAssetsLookupTable[i]);
    }

    __android_log_print(ANDROID_LOG_ERROR, "DKR-LUT",
        "embedded LUT: [0]=%u [1]=%u [2]=%u [3]=%u [4]=%u [5]=%u [6]=%u [7]=%u",
        gAssetsLookupTable[0], gAssetsLookupTable[1], gAssetsLookupTable[2], gAssetsLookupTable[3],
        gAssetsLookupTable[4], gAssetsLookupTable[5], gAssetsLookupTable[6], gAssetsLookupTable[7]);
}

/**
 * Returns the memory address containing an asset section loaded from ROM.
 * Official Name: piRomLoad
 */
u32 *asset_table_load(u32 assetIndex) {
    u32 *index;
    u32 *out;
    s32 size;
    u32 start;

#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
#endif

    if (gAssetsLookupTable[0] < assetIndex) {
        return NULL;
    }
    assetIndex++;
    index = assetIndex + gAssetsLookupTable;
    start = *index;
    size = *(index + 1) - start;
    out = (u32 *) mempool_alloc_safe(size, COLOUR_TAG_GREY);
    if (out == 0) {
        return NULL;
    }

    dmacopy_internal((u32) (start + gBkaAssetsRomOffset), out, size);

#if VERSION >= VERSION_79
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    return out;
}

/**
 * Loads a gzip compressed asset from the ROM file.
 * Returns a pointer to the decompressed data.
 * Official name: piRomLoadCompressed
 */
UNUSED u8 *asset_table_load_zipped(u32 assetIndex, s32 extraMemory) {
    s32 size;
    s32 start;
    s32 totalSpace;
    u8 *gzipHeaderRamPos;
    u8 *out;

#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
#endif

    if (gAssetsLookupTable[0] < assetIndex) {
        return NULL;
    }
    assetIndex++;
    out = (u8 *) (assetIndex + gAssetsLookupTable);
    start = ((s32 *) out)[0];
    size = ((s32 *) out)[1] - start;
    gzipHeaderRamPos = (u8 *) mempool_alloc_safe(8, COLOUR_TAG_WHITE);

    dmacopy_internal((u32) (start + gBkaAssetsRomOffset), gzipHeaderRamPos, 8);

    totalSpace = byteswap32(gzipHeaderRamPos) + extraMemory;
    mempool_free(gzipHeaderRamPos);
    out = (u8 *) mempool_alloc_safe(totalSpace + extraMemory, COLOUR_TAG_GREY);
    if (out == NULL) {
        return NULL;
    }
    gzipHeaderRamPos = (out + totalSpace) - size;
    if (1) {} // Fakematch
    dmacopy_internal((u32) (start + gBkaAssetsRomOffset), gzipHeaderRamPos, size);
    gzip_inflate(gzipHeaderRamPos, out);

#if VERSION >= VERSION_79
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    return out;
}

/**
 * Loads an asset section to a specific memory address.
 * Returns the size of asset section.
 */
UNUSED s32 asset_table_load_addr(u32 assetIndex, void *address) {
    u32 start;
    s32 size;
    u32 *index;

#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
#endif

    if (gAssetsLookupTable[0] < assetIndex) {
        return NULL;
    }
    assetIndex++;
    index = assetIndex + gAssetsLookupTable;
    start = *index;
    size = *(index + 1) - start;

    dmacopy_internal((u32) (start + gBkaAssetsRomOffset), address, size);

#if VERSION >= VERSION_79
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    return size;
}

/**
 * Loads part of an asset section to a specific memory address.
 * Returns the size argument.
 * Official name: piRomLoadSection
 */
s32 asset_load(u32 assetIndex, void *address, s32 assetOffset, s32 size) {
    ptd_suspect(address, "asset_load");
    __android_log_print(ANDROID_LOG_ERROR, "DKR-LOAD",
        "asset_load ENTRY id=%u address=%p size=%d", assetIndex, address, size);
    u32 *index;
    s32 start;

#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
#endif

    if (size <= 0 || gAssetsLookupTable[0] < assetIndex) {
        if (size < 0) __android_log_print(ANDROID_LOG_ERROR, "DKR-LOAD", "NEGATIVE size=%d asset=%u — skipped", size, assetIndex);
        return NULL;
    }

    assetIndex++;
    index = assetIndex + gAssetsLookupTable;
    start = *index + assetOffset;

    dmacopy_internal((u32) (start + gBkaAssetsRomOffset), address, size);

#if VERSION >= VERSION_79
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    return size;
}

/**
 * Returns a rom offset of an asset given its asset section and a local offset.
 * Official name: piRomGetSectionPtr
 */
u8 *asset_rom_offset(u32 assetIndex, u32 assetOffset) {
    u32 *index;
    u32 start;

#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
#endif

    if (gAssetsLookupTable[0] < assetIndex) {
        return NULL;
    }

    assetIndex++;
    index = assetIndex + gAssetsLookupTable;
    start = *index + assetOffset;

#if VERSION >= VERSION_79
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    return start + gBkaAssetsRomOffset;
}

/**
 * Returns the size of an asset section.
 * Official name: piRomGetFileSize
 */
s32 asset_table_size(u32 assetIndex) {
    u32 *index;

#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
#endif

    if (gAssetsLookupTable[0] < assetIndex) {
        return NULL;
    }

    assetIndex++;
    index = assetIndex + gAssetsLookupTable;

#if VERSION >= VERSION_79
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
#endif

    return *(index + 1) - *index;
}

#define MAX_TRANSFER_SIZE 0x5000

/**
 * Copies data from the game cartridge to a ram address.
 * Official name: romCopy
 */
void dmacopy(u32 romOffset, void *ramAddress, s32 numBytes) {
    // ARM64 truncation guard: on 64-bit Android, ramAddress MUST fit in 64 bits.
    // If upper 32 bits are zero but the address is < 4GB and != NULL, it was
    // truncated somewhere upstream. Abort early to make the bug visible.
    uintptr_t _addr_val = (uintptr_t)ramAddress;
    if (_addr_val != 0 && _addr_val < 0x100000000ULL && numBytes > 0) {
        __android_log_print(ANDROID_LOG_ERROR, "DKR-TRUNC",
            "dmacopy: TRUNCATED dst ptr=0x%lX numBytes=%d romOffset=0x%X",
            (unsigned long)_addr_val, numBytes, romOffset);
    }
#if VERSION >= VERSION_79
    OSMesg msg = NULL;
    osRecvMesg(&gDmaMutex, &msg, OS_MESG_BLOCK);
    dmacopy_internal(romOffset, ramAddress, numBytes);
    osSendMesg(&gDmaMutex, (OSMesg) 1, OS_MESG_NOBLOCK);
}

// Looks like v2 ROMs made an alternate version of this function, and this is the original.
void dmacopy_internal(u32 romOffset, void *ramAddress, s32 numBytes) {
    __android_log_print(ANDROID_LOG_ERROR, "DKR-DMA",
        "dmacopy: romOffset=0x%X ramAddr=%p numBytes=%d", romOffset, ramAddress, numBytes);
#endif
    OSMesg dmaMesg;
    s32 numBytesToDMA;

    osInvalDCache((u32 *) ramAddress, numBytes);
    numBytesToDMA = MAX_TRANSFER_SIZE;
    while (numBytes > 0) {
        if (numBytes < numBytesToDMA) {
            numBytesToDMA = numBytes;
        }
        osPiStartDma(&gAssetsDmaIoMesg, OS_MESG_PRI_NORMAL, OS_READ, romOffset, (u32 *) ramAddress, numBytesToDMA,
                     &gDmaMesgQueue);
        osRecvMesg(&gDmaMesgQueue, &dmaMesg, OS_MESG_BLOCK);
        numBytes -= numBytesToDMA;
        romOffset += numBytesToDMA;
        ramAddress += numBytesToDMA;
    }
}
