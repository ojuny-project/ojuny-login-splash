#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Assets {
    uint32_t bg_rgb;
    uint32_t logo_w;
    uint32_t logo_h;
    uint32_t logo_content;
    uint32_t text_w;
    uint32_t text_h;
    uint32_t text_gap;
    const uint8_t *logo;
    const uint8_t *text;
    void *logo_map;
    void *text_map;
    size_t logo_map_size;
    size_t text_map_size;
} Assets;

bool assets_load(Assets *assets);
void assets_unload(Assets *assets);
