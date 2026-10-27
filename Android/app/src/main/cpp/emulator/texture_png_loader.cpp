#include <jni.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <mutex>
#include <atomic>
#include <android/log.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef _ULTRATYPES_H_
#define _ULTRATYPES_H_
typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef unsigned long long u64;
typedef signed char        s8;
typedef short              s16;
typedef int                s32;
typedef long long          s64;
typedef float              f32;
typedef double             f64;
#endif

extern "C" {
#include "../../../../../../include/structs.h"

#include <cstdarg>
static void diag_log(const char* tag, const char* fmt, ...) {
    FILE* f = fopen("/data/user/0/com.dkrwrapper/files/diag.log", "a");
    if (!f) return;
    fprintf(f, "%s: ", tag);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

}

#define LOG_TAG "BKA-PNG"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace fs = std::filesystem;

static std::unordered_map<uint32_t, TextureHeader*> g_texture_cache_3d;
static std::unordered_map<uint32_t, TextureHeader*> g_texture_cache_2d;
static std::unordered_map<uint32_t, std::string> g_png_paths_3d;
static std::unordered_map<uint32_t, std::string> g_png_paths_2d;

static std::string g_base_path = "/data/data/com.dkrwrapper/files/extractor/assets/.vanilla/us.v80/";
static std::atomic<bool> g_initialized{false};
static std::mutex g_init_mutex;

static std::string normalize_name(const std::string& name) {
    std::string s = name;
    static const std::string p3 = "ASSET_TEX3D_";
    static const std::string p2 = "ASSET_TEX2D_";
    if (s.compare(0, p3.size(), p3) == 0) s = s.substr(p3.size());
    else if (s.compare(0, p2.size(), p2) == 0) s = s.substr(p2.size());
    std::string out;
    for (char c : s) {
        unsigned char uc = (unsigned char)c;
        if (std::isalnum(uc)) out += (char)std::tolower(uc);
    }
    return out;
}

static std::vector<std::string> read_order_list(const fs::path& json_path) {
    std::vector<std::string> result;
    std::ifstream f(json_path);
    if (!f) return result;
    std::stringstream ss; ss << f.rdbuf();
    const std::string content = ss.str();
    size_t pos = content.find("\"order\"");
    if (pos == std::string::npos) return result;
    pos = content.find('[', pos);
    if (pos == std::string::npos) return result;
    ++pos;
    while (pos < content.size()) {
        while (pos < content.size() && (std::isspace((unsigned char)content[pos]) || content[pos] == ',')) ++pos;
        if (pos >= content.size() || content[pos] == ']') break;
        if (content[pos] != '"') break;
        ++pos;
        size_t end = content.find('"', pos);
        if (end == std::string::npos) break;
        result.push_back(content.substr(pos, end - pos));
        pos = end + 1;
    }
    return result;
}

static void scan_dir(const fs::path& dir, const std::string& strip_marker,
                     std::unordered_map<std::string, std::string>& out) {
    if (!fs::exists(dir)) return;
    std::error_code ec;
    for (auto it = fs::recursive_directory_iterator(dir,
              fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        const auto& entry = *it;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".png") continue;
        std::string path_str = entry.path().string();
        auto p = path_str.find(strip_marker);
        if (p == std::string::npos) continue;
        std::string rel = path_str.substr(p + strip_marker.size());
        if (rel.size() > 4 && rel.compare(rel.size() - 4, 4, ".png") == 0) rel.resize(rel.size() - 4);
        std::string key = normalize_name(rel);
        if (!key.empty()) out[key] = entry.path().string();
    }
}

static void do_init() {
    diag_log("BKA-PNG-INIT", "root=%s", g_base_path.c_str());
    LOGI("Init root=%s", g_base_path.c_str());
    fs::path root(g_base_path);
    std::unordered_map<std::string, std::string> norm_3d, norm_2d;
    scan_dir(root / "textures" / "3d", "/textures/3d/", norm_3d);
    scan_dir(root / "textures" / "2d", "/textures/2d/", norm_2d);
    auto order_3d = read_order_list(root / "asset_textures_3d.meta.json");
    auto order_2d = read_order_list(root / "asset_textures_2d.meta.json");
    for (size_t i = 0; i < order_3d.size(); ++i) {
        auto it = norm_3d.find(normalize_name(order_3d[i]));
        if (it != norm_3d.end()) g_png_paths_3d[(uint32_t)i] = it->second;
    }
    for (size_t i = 0; i < order_2d.size(); ++i) {
        auto it = norm_2d.find(normalize_name(order_2d[i]));
        if (it != norm_2d.end()) g_png_paths_2d[(uint32_t)i] = it->second;
    }
    LOGI("Mapped: 3D %zu/%zu  2D %zu/%zu",
         g_png_paths_3d.size(), order_3d.size(),
         g_png_paths_2d.size(), order_2d.size());
}

extern "C" void init_png_texture_loader(const char* base_path) {
    if (base_path && *base_path) g_base_path = base_path;
}

static void ensure_initialized() {
    if (g_initialized.load()) return;
    std::lock_guard<std::mutex> lk(g_init_mutex);
    if (g_initialized.load()) return;
    do_init();
    if (!g_png_paths_3d.empty() || !g_png_paths_2d.empty()) {
        g_initialized.store(true);
    }
}

// --- Convert RGBA8 to a 16-bit N64 RGBA5551 word ---
static inline uint16_t rgba8_to_n64(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >= 128 ? 1 : 0);
}

static TextureHeader* load_png_texture(uint32_t id,
        std::unordered_map<uint32_t, std::string>& paths,
        std::unordered_map<uint32_t, TextureHeader*>& cache) {
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;
    auto pit = paths.find(id);
    if (pit == paths.end()) return nullptr;

    int w = 0, h = 0, ch = 0;
    unsigned char* data = stbi_load(pit->second.c_str(), &w, &h, &ch, 4);
    if (!data) { LOGE("stbi_load failed: %s", pit->second.c_str()); return nullptr; }

    size_t header_size = sizeof(TextureHeader);
    size_t pixel_size = (size_t)w * (size_t)h * 2;
    TextureHeader* header = (TextureHeader*)bka_rdram_alloc(header_size + pixel_size);
    if (header) memset(header, 0, header_size + pixel_size);
    if (!header) { stbi_image_free(data); return nullptr; }

    // RGBA16 format = 1 in DKR's enum (0=RGBA32, 1=RGBA16, 2=I8, etc.)
    header->width              = (u8)w;
    header->height             = (u8)h;
    header->format             = 1;             // RGBA16
    header->numberOfInstances  = 1;
    header->numberOfCommands   = 0x07;
    header->numOfTextures      = 1;
    header->textureSize        = (s16)(header_size + pixel_size);

    uint16_t* pixels = (uint16_t*)((uint8_t*)header + header_size);

    // --- Build display list to upload texture to TMEM ---
    // The game relies on header->cmd to copy pixels from RDRAM to TMEM.
    // Since we bypass the ROM, we must construct this list manually.
    // Build display list via raw uint32_t writes — works regardless of Gfx struct field names.
    // Standard N64 Gfx = 8 bytes (w0, w1). We assume sizeof(Gfx) == 8, which is universal.
    if (sizeof(Gfx) == 8) {
        Gfx* cmd = (Gfx*)bka_rdram_alloc(5 * sizeof(Gfx));
        if (cmd) {
            uint32_t* cw = (uint32_t*)cmd;
            uintptr_t phys_px = (uintptr_t)pixels - (uintptr_t)gN64_RDRAM;
            uint32_t pixel_addr = 0x80000000u | (uint32_t)phys_px;

            // G_SETTIMG (0xFD): SetTextureImage
            cw[0] = (0xFDu << 24) | (0u << 21) | (2u << 19) | (w - 1);
            cw[1] = pixel_addr;

            // G_SETTILE (0xF5): SetTile — RGBA, 16b, line=w*2
            cw[2] = (0xF5u << 24) | (0u << 21) | (2u << 19) | ((uint32_t)(w * 2) << 9) | 0u;
            cw[3] = 0;

            // G_SETTILESIZE (0xF2): SetTileSize — uls=0, ult=0, lrs=w-1, lrt=h-1
            cw[4] = (0xF2u << 24) | (0u << 12) | 0u;
            cw[5] = (0u << 24) | ((uint32_t)(w - 1) << 12) | (uint32_t)(h - 1);

            // G_LOADBLOCK (0xF3): LoadBlock — uls=0, ult=0, texels = w*h-1
            cw[6] = (0xF3u << 24) | (0u << 12) | 0u;
            cw[7] = (0u << 24) | (0u << 12) | ((uint32_t)(w * h) - 1);

            // G_ENDDL (0xB8)
            cw[8] = (0xB8u << 24);
            cw[9] = 0;

            // Set cmd to K0 pointer so the emulator's walker can find it
            uintptr_t phys_cmd = (uintptr_t)cmd - (uintptr_t)gN64_RDRAM;
            header->cmd = (Gfx*)(0x80000000u | (uint32_t)phys_cmd);
        }
    }
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = data[i*4+0], g = data[i*4+1], b = data[i*4+2], a = data[i*4+3];
        pixels[i] = rgba8_to_n64(r, g, b, a);
    }
    stbi_image_free(data);
    cache[id] = header;
    return header;
}

extern "C" TextureHeader* png_load_texture_3d(uint32_t id) {
    ensure_initialized();
    TextureHeader* h = load_png_texture(id, g_png_paths_3d, g_texture_cache_3d);
    static int _dbg = 0;
    if (_dbg++ < 20) diag_log("BKA-PNG-FETCH", "id=0x%X -> %p", id, h);
    return h;
}
extern "C" TextureHeader* png_load_texture_2d(uint32_t id) {
    ensure_initialized();
    return load_png_texture(id, g_png_paths_2d, g_texture_cache_2d);
}
