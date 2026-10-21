#include "ptr_shim.h"
#include <stdlib.h>
#include <android/log.h>
#include <pthread.h>

#define MAX_REGIONS 65536
typedef struct { uintptr_t base; size_t size; } region_t;

static region_t g_regions[MAX_REGIONS];
static int g_count = 0;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void ptd_register_alloc(void *ptr, size_t size) {
    if (!ptr || !size) return;
    pthread_mutex_lock(&g_lock);
    if (g_count < MAX_REGIONS) {
        g_regions[g_count].base = (uintptr_t)ptr;
        g_regions[g_count].size = size;
        g_count++;
    }
    pthread_mutex_unlock(&g_lock);
}

int ptd_suspect(void *ptr, const char *context) {
    if (!ptr) return 0;
    uintptr_t v = (uintptr_t)ptr;
    /* Heap/stack/code addresses on ARM64 are all above 4GB */
    if (v > 0x100000000ULL) return 0;

    pthread_mutex_lock(&g_lock);
    for (int i = 0; i < g_count; i++) {
        uintptr_t low  = g_regions[i].base & 0xFFFFFFFFULL;
        uintptr_t high = (g_regions[i].base + g_regions[i].size) & 0xFFFFFFFFULL;
        if (low <= high) {
            if (v >= low - 4096 && v <= high + 4096) {
                __android_log_print(ANDROID_LOG_ERROR, "PTD",
                    "SUSPECTED TRUNCATION in %s: ptr=0x%lX matches alloc 0x%lX..0x%lX (low32 0x%lX..0x%lX)",
                    context, (unsigned long)v,
                    (unsigned long)g_regions[i].base,
                    (unsigned long)(g_regions[i].base + g_regions[i].size),
                    (unsigned long)low, (unsigned long)high);
                pthread_mutex_unlock(&g_lock);
                return 1;
            }
        }
    }
    pthread_mutex_unlock(&g_lock);
    return 0;
}
