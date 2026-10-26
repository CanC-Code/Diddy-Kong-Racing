#include <jni.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <android/log.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Pre-define N64 integer types BEFORE any DKR headers.
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
}

#define LOG_TAG "BKA-PNG"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace fs = std::filesystem;

static std::unordered_map<uint32_t, TextureHeader*> g_texture_cache_3d;
static std::unordered_map<uint32_t, TextureHeader*> g_texture_cache_2d;
static std::unordered_map<uint32_t, std::string> g_png_paths_3d;
static std::unordered_map<uint32_t, std::string> g_png_paths_2d;

// Normalize: strip ASSET_TEX3D_/2D_ prefix, lowercase, keep only alnum.
static std::string normalize_name(const std::string& name) {
    std::string s = name;
    static const std::string p3 = "ASSET_TEX3D_";
    static const std::string p2 = "ASSET_TEX2D_";
    if (s.compare(0, p3.size(), p3) == 0) s = s.substr(p3.size());
    else if (s.compare(0, p2.size(), p2) == 0) s = s.substr(p2.size());
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        unsigned char uc = (unsigned char)c;
        if (std::isalnum(uc)) out += (char)std::tolower(uc);
    }
    return out;
}

// Parse the "order" array from a meta JSON file: ["NAME1","NAME2",...]
static std::vector<std::string> read_order_list(const fs::path& json_path) {
    std::vector<std::string> result;
    std::ifstream f(json_path);
    if (!f) { LOGE("Cannot open %s", json_path.string().c_str()); return result; }
    std::stringstream ss; ss << f.rdbuf();
    const std::string content = ss.str();
    size_t pos = content.find("\"order\"");
    if (pos == std::string::npos) { LOGE("No 'order' key in %s", json_path.string().c_str()); return result; }
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

// Recursively scan a directory for .png; store normalized relative key -> full path.
static void scan_dir(const fs::path& dir, const std::string& strip_marker,
                     std::unordered_map<std::string, std::string>& out) {
    if (!fs::exists(dir)) return;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
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

extern "C" void init_png_texture_loader(const char* base_path) {
    fs::path root(base_path);
    LOGI("Init: root=%s", base_path);

    std::unordered_map<std::string, std::string> norm_3d, norm_2d;
    scan_dir(root / "textures" / "3d", "/textures/3d/", norm_3d);
    scan_dir(root / "textures" / "2d", "/textures/2d/", norm_2d);
    LOGI("Scanned: %zu unique 3D keys, %zu unique 2D keys", norm_3d.size(), norm_2d.size());

    auto order_3d = read_order_list(root / "asset_textures_3d.meta.json");
    auto order_2d = read_order_list(root / "asset_textures_2d.meta.json");
    LOGI("Order lists: %zu 3D, %zu 2D", order_3d.size(), order_2d.size());

    size_t miss3 = 0, miss2 = 0;
    for (size_t i = 0; i < order_3d.size(); ++i) {
        auto it = norm_3d.find(normalize_name(order_3d[i]));
        if (it != norm_3d.end()) g_png_paths_3d[(uint32_t)i] = it->second;
        else ++miss3;
    }
    for (size_t i = 0; i < order_2d.size(); ++i) {
        auto it = norm_2d.find(normalize_name(order_2d[i]));
        if (it != norm_2d.end()) g_png_paths_2d[(uint32_t)i] = it->second;
        else ++miss2;
    }

    LOGI("PNG loader ready: 3D %zu/%zu mapped (%zu missing), 2D %zu/%zu mapped (%zu missing)",
         g_png_paths_3d.size(), order_3d.size(), miss3,
         g_png_paths_2d.size(), order_2d.size(), miss2);

    // Log the sizeof TextureHeader once so we can verify at runtime
    LOGI("sizeof(TextureHeader)=%zu", sizeof(TextureHeader));
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
    TextureHeader* header = (TextureHeader*)calloc(1, header_size + pixel_size);
    if (!header) { stbi_image_free(data); return nullptr; }

    header->width              = (u8)w;
    header->height             = (u8)h;
    header->format             = 1;               // RGBA16
    header->numberOfInstances  = 1;
    header->numberOfCommands   = 0x07;
    header->numOfTextures      = 1;
    header->frameAdvanceDelay  = 0;
    header->textureSize        = (s16)(header_size + pixel_size);
    header->isCompressed       = 0;

    uint16_t* pixels = (uint16_t*)((uint8_t*)header + header_size);
    for (int i = 0; i < w * h; ++i) {
        uint8_t r = data[i*4+0], g = data[i*4+1], b = data[i*4+2], a = data[i*4+3];
        uint16_t v = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a >= 128 ? 1 : 0);
        pixels[i] = v;
    }
    stbi_image_free(data);
    cache[id] = header;
    return header;
}

extern "C" TextureHeader* png_load_texture_3d(uint32_t id) {
    return load_png_texture(id, g_png_paths_3d, g_texture_cache_3d);
}
extern "C" TextureHeader* png_load_texture_2d(uint32_t id) {
    return load_png_texture(id, g_png_paths_2d, g_texture_cache_2d);
}
