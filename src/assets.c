#include "assets.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "util.h"

#ifndef OJUNY_ASSET_DIR
#define OJUNY_ASSET_DIR "build/assets"
#endif

static bool read_meta(uint32_t values[8]) {
    char path[512];
    snprintf(path, sizeof(path), "%s/meta", OJUNY_ASSET_DIR);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        log_error("cannot open %s: %s", path, strerror(errno));
        return false;
    }
    unsigned char buffer[32];
    size_t offset = 0;
    while (offset < sizeof(buffer)) {
        ssize_t got = read(fd, buffer + offset, sizeof(buffer) - offset);
        if (got < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("cannot read %s: %s", path, strerror(errno));
            close(fd);
            return false;
        }
        if (got == 0) {
            break;
        }
        offset += (size_t)got;
    }
    close(fd);
    if (offset != sizeof(buffer)) {
        log_error("short asset metadata: %s", path);
        return false;
    }
    for (int i = 0; i < 8; ++i) {
        values[i] = (uint32_t)buffer[i * 4] |
                    ((uint32_t)buffer[i * 4 + 1] << 8) |
                    ((uint32_t)buffer[i * 4 + 2] << 16) |
                    ((uint32_t)buffer[i * 4 + 3] << 24);
    }
    return true;
}

static bool map_asset(const char *name, size_t expected, const uint8_t **pointer,
                      void **map, size_t *map_size) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", OJUNY_ASSET_DIR, name);
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        log_error("cannot open %s: %s", path, strerror(errno));
        return false;
    }
    struct stat stat_buffer;
    if (fstat(fd, &stat_buffer) != 0) {
        log_error("cannot stat %s: %s", path, strerror(errno));
        close(fd);
        return false;
    }
    if ((size_t)stat_buffer.st_size != expected) {
        log_error("unexpected size for %s: %lld", path, (long long)stat_buffer.st_size);
        close(fd);
        return false;
    }
    void *mapped = mmap(NULL, expected, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (mapped == MAP_FAILED) {
        log_error("cannot map %s: %s", path, strerror(errno));
        return false;
    }
    *pointer = mapped;
    *map = mapped;
    *map_size = expected;
    return true;
}

bool assets_load(Assets *assets) {
    memset(assets, 0, sizeof(*assets));

    uint32_t meta[8];
    if (!read_meta(meta)) {
        return false;
    }
    if (meta[0] != 1) {
        log_error("unsupported asset version %u", meta[0]);
        return false;
    }

    uint32_t logo_w = meta[2];
    uint32_t logo_h = meta[3];
    uint32_t text_w = meta[5];
    uint32_t text_h = meta[6];
    if (logo_w == 0 || logo_h == 0 || logo_w > 16384 || logo_h > 16384 ||
        text_w == 0 || text_h == 0 || text_w > 16384 || text_h > 16384) {
        log_error("invalid asset dimensions");
        return false;
    }
    if (meta[4] == 0 || meta[4] > logo_w) {
        log_error("invalid logo content size");
        return false;
    }
    if (meta[7] > 4096) {
        log_error("invalid text gap");
        return false;
    }

    assets->bg_rgb = meta[1] & 0xFFFFFFu;
    assets->logo_w = logo_w;
    assets->logo_h = logo_h;
    assets->logo_content = meta[4];
    assets->text_w = text_w;
    assets->text_h = text_h;
    assets->text_gap = meta[7];

    size_t logo_size = (size_t)logo_w * (size_t)logo_h * 4u;
    size_t text_size = (size_t)text_w * (size_t)text_h * 4u;

    if (!map_asset("logo.rgba", logo_size, &assets->logo, &assets->logo_map,
                   &assets->logo_map_size)) {
        assets_unload(assets);
        return false;
    }
    if (!map_asset("text.rgba", text_size, &assets->text, &assets->text_map,
                   &assets->text_map_size)) {
        assets_unload(assets);
        return false;
    }
    return true;
}

void assets_unload(Assets *assets) {
    if (assets->logo_map) {
        munmap(assets->logo_map, assets->logo_map_size);
    }
    if (assets->text_map) {
        munmap(assets->text_map, assets->text_map_size);
    }
    memset(assets, 0, sizeof(*assets));
}
