#include <jni.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <android/log.h>
#include "stb_image.h"

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

uint32_t extract_id_from_filename(const std::string& filename) {
    size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos) return 0;
    size_t underscore = filename.find_last_of('_', dot);
    if (underscore == std::string::npos) return 0;
    
    try {
        return std::stoul(filename.substr(underscore + 1, dot - underscore - 1));
    } catch (...) {
        return 0;
    }
}

extern "C" void init_png_texture_loader(const char* base_path) {
    fs::path root(base_path);
    fs::path path_3d = root / "textures" / "3d";
    fs::path path_2d = root / "textures" / "2d";

    auto scan_dir = [](const fs::path& dir, std::unordered_map<uint32_t, std::string>& out_map) {
        if (!fs::exists(dir)) return;
        for (const auto& entry : fs::recursive_directory_iterator(dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".png") {
                uint32_t id = extract_id_from_filename(entry.path().filename().string());
                if (id != 0) {
                    out_map[id] = entry.path().string();
                }
            }
        }
    };

    scan_dir(path_3d, g_png_paths_3d);
    scan_dir(path_2d, g_png_paths_2d);
    
    LOGI("PNG Loader initialized. Found %zu 3D textures, %zu 2D textures.", 
         g_png_paths_3d.size(), g_png_paths_2d.size());
}

TextureHeader* load_png_texture(uint32_t id, const std::unordered_map<uint32_t, std::string>& paths, std::unordered_map<uint32_t, TextureHeader*>& cache) {
    auto it = cache.find(id);
    if (it != cache.end()) {
        return it->second;
    }

    auto path_it = paths.find(id);
    if (path_it == paths.end()) {
        return nullptr; 
    }

    int width, height, channels;
    unsigned char* data = stbi_load(path_it->second.c_str(), &width, &height, &channels, 4);
    if (!data) {
        LOGE("Failed to load PNG: %s", path_it->second.c_str());
        return nullptr;
    }

    size_t header_size = sizeof(TextureHeader);
    size_t pixel_size = width * height * 2; 
    
    TextureHeader* header = (TextureHeader*)malloc(header_size + pixel_size);
    if (!header) {
        stbi_image_free(data);
        return nullptr;
    }

    header->width = width;
    header->height = height;
    header->format = 1; // RGBA16 format
    header->isCompressed = 0;
    header->uncompressedSize = pixel_size;
    header->flags = 0; 
    
    uint16_t* pixels16 = (uint16_t*)((uint8_t*)header + header_size);
    for (int i = 0; i < width * height; i++) {
        uint8_t r = data[i * 4 + 0];
        uint8_t g = data[i * 4 + 1];
        uint8_t b = data[i * 4 + 2];
        uint8_t a = data[i * 4 + 3];
        
        uint16_t rgba16 = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (a > 0 ? 1 : 0);
        pixels16[i] = rgba16;
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
