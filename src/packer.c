#define DEBUG
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "assets.h"
#include "raylib.h"

pak_entry entries[ASSET_COUNT] = {0};
int offset = 0;

void pack(int type) {
    const char *path = ASSET(type);
    int size = 0;
    if (!FileExists(path)) {
        printf("File %s does not exists\n", path);
        exit(1);
    } else {
        entries[type].content = LoadFileData(path, &size);
    }
    entries[type].size = (uint32_t)size;
    entries[type].offset = offset;
    entries[type].type = type;
    offset += size;
}

// TODO: Add compression ?
int main(void) {
    SetTraceLogLevel(LOG_NONE);
    for (int i = 0; i < ASSET_COUNT; i++) {
        pack(i);
    }

    FILE *f = fopen("assets.pak", "wb");
    if (f == NULL) {
        printf("Could not open file\n");
        return 1;
    }

    for (int i = 0; i < ASSET_COUNT; i++) {
        fwrite(&entries[i].offset, sizeof(uint32_t), 1, f);
        fwrite(&entries[i].size, sizeof(uint32_t), 1, f);
    }
    for (int i = 0; i < ASSET_COUNT; i++) {
        fwrite(entries[i].content, sizeof(char), entries[i].size, f);
        printf("Packed %s at %u with size %u\n", ASSET(i), entries[i].offset, entries[i].size);
    }
    return 0;
}
