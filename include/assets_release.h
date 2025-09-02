#ifndef ASSET2_IMPL
#define ASSET2_IMPL

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "assets.h"
#include "common.h"
#include "raylib.h"

pak_entry entries[ASSET_COUNT] = {0};

bool packer_loaded = false;

void load_packer(const char* path) {
    FILE* f = fopen(path, "rb");
    if (f == NULL) {
        printf("Can't read %s\n", path);
        exit(1);
    }
    for (int i = 0; i < ASSET_COUNT; i++) {
        fread(&entries[i].offset, sizeof(uint32_t), 1, f);
        fread(&entries[i].size, sizeof(uint32_t), 1, f);
    }
    for (int i = 0; i < ASSET_COUNT; i++) {
        entries[i].content = malloc(entries[i].size);
        fread(entries[i].content, sizeof(unsigned char), entries[i].size, f);
    }
    fclose(f);
    packer_loaded = true;
}

pak_entry ASSET2(asset a) {
    if (packer_loaded == false) {
        load_packer("assets.pak");
    }
    LOG("Loading %s from packer !!", ASSET(a));
    return entries[a];
}

#endif
