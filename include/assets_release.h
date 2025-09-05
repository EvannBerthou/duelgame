#ifndef ASSET_IMPL
#define ASSET_IMPL

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "assets.h"
#include "common.h"
#include "raylib.h"

#ifdef EMBED_ASSETS
#include "assets_packed.h"
#endif

pak_entry entries[ASSET_COUNT] = {0};

bool packer_loaded = false;

#ifdef EMBED_ASSETS
void load_packer_from_memory() {
    int ptr = 0;
    for (int i = 0; i < ASSET_COUNT; i++) {
        memcpy(&entries[i].offset, assets_pak + ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        memcpy(&entries[i].size, assets_pak + ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
    }
    int base_offset = ASSET_COUNT * 2 * sizeof(uint32_t);
    for (int i = 0; i < ASSET_COUNT; i++) {
        entries[i].content = malloc(entries[i].size);
        memcpy(entries[i].content, assets_pak + base_offset + entries[i].offset, entries[i].size);
    }
    LOG("Loaded %d assets from memory.", ASSET_COUNT);
    packer_loaded = true;
}
#endif

void load_packer_from_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        printf("Can't read %s\n", path);
        exit(1);
    }
    for (int i = 0; i < ASSET_COUNT; i++) {
        fread(&entries[i].offset, sizeof(uint32_t), 1, f);
        fread(&entries[i].size, sizeof(uint32_t), 1, f);
    }
    int base_offset = ASSET_COUNT * 2 * sizeof(uint32_t);

    for (int i = 0; i < ASSET_COUNT; i++) {
        entries[i].content = malloc(entries[i].size);
        fseek(f, base_offset + entries[i].offset, SEEK_SET);
        fread(entries[i].content, sizeof(unsigned char), entries[i].size, f);
    }

    fclose(f);
    packer_loaded = true;
}

pak_entry ASSET(asset a) {
#ifdef EMBED_ASSETS
    if (packer_loaded == false) {
        load_packer_from_memory();
    }
#else
    if (packer_loaded == false) {
        load_packer_from_file("assets.pak");
    }
#endif
    LOG("Loading %d from packer.", a);
    return entries[a];
}

Font load_font(asset type) {
    pak_entry entry = ASSET(type);
    return LoadFontFromMemory(".ttf", entry.content, entry.size, 16, NULL, 0);
}

Sound load_sound(asset type) {
    pak_entry entry = ASSET(type);
    Wave wave = LoadWaveFromMemory(".wav", entry.content, entry.size);
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

Texture2D load_texture(asset type) {
    pak_entry entry = ASSET(type);
    Image image = LoadImageFromMemory(".png", entry.content, entry.size);
    Texture2D texture = LoadTextureFromImage(image);
    UnloadImage(image);
    return texture;
}

#endif
