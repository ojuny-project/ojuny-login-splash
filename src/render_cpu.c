#include "render.h"

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "app.h"
#include "util.h"

typedef struct Cover {
    surface *surface;
    struct wl_buffer *buffer;
    uint8_t *data;
    size_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
} Cover;

static void blend_image(uint32_t *destination, int destination_width,
                        int destination_height, const uint8_t *source,
                        int source_width, int source_height, int origin_x,
                        int origin_y) {
    for (int y = 0; y < source_height; ++y) {
        int target_y = origin_y + y;
        if (target_y < 0 || target_y >= destination_height) {
            continue;
        }
        const uint8_t *row = source + (size_t)y * (size_t)source_width * 4u;
        for (int x = 0; x < source_width; ++x) {
            int target_x = origin_x + x;
            if (target_x < 0 || target_x >= destination_width) {
                continue;
            }
            const uint8_t *pixel = row + (size_t)x * 4u;
            unsigned alpha = pixel[3];
            if (alpha == 0) {
                continue;
            }
            size_t index = (size_t)target_y * (size_t)destination_width +
                           (size_t)target_x;
            uint32_t value = destination[index];
            unsigned red = (value >> 16) & 255u;
            unsigned green = (value >> 8) & 255u;
            unsigned blue = value & 255u;
            if (alpha == 255u) {
                red = pixel[0];
                green = pixel[1];
                blue = pixel[2];
            } else {
                red = (pixel[0] * alpha + red * (255u - alpha)) / 255u;
                green = (pixel[1] * alpha + green * (255u - alpha)) / 255u;
                blue = (pixel[2] * alpha + blue * (255u - alpha)) / 255u;
            }
            destination[index] =
                0xFF000000u | (red << 16) | (green << 8) | blue;
        }
    }
}

static void paint(Assets *assets, uint32_t *pixels, uint32_t width,
                  uint32_t height, bool with_logo) {
    uint32_t background = 0xFF000000u | (assets->bg_rgb & 0xFFFFFFu);
    size_t count = (size_t)width * (size_t)height;
    for (size_t index = 0; index < count; ++index) {
        pixels[index] = background;
    }
    if (!with_logo) {
        return;
    }

    int logo_x = ((int)width - (int)assets->logo_w) / 2;
    int logo_y = ((int)height - (int)assets->logo_h) / 2;
    blend_image(pixels, (int)width, (int)height, assets->logo,
                (int)assets->logo_w, (int)assets->logo_h, logo_x, logo_y);

    int text_x = ((int)width - (int)assets->text_w) / 2;
    int text_y = (int)(height / 2u) + (int)(assets->logo_content / 2u) +
                 (int)assets->text_gap;
    blend_image(pixels, (int)width, (int)height, assets->text,
                (int)assets->text_w, (int)assets->text_h, text_x, text_y);
}

static void cover_release(void *data, struct wl_buffer *buffer) {
    Cover *cover = data;
    wl_buffer_destroy(buffer);
    if (cover->data) {
        munmap(cover->data, cover->size);
    }
    if (cover->surface && cover->surface->cover == cover) {
        cover->surface->cover = NULL;
    }
    free(cover);
}

static const struct wl_buffer_listener cover_listener = {
    .release = cover_release,
};

static void set_opaque_region(app *application, surface *draw_surface) {
    struct wl_region *region =
        wl_compositor_create_region(application->compositor);
    if (!region) {
        return;
    }
    wl_region_add(region, 0, 0, (int32_t)draw_surface->width,
                  (int32_t)draw_surface->height);
    wl_surface_set_opaque_region(draw_surface->wl_surface, region);
    wl_region_destroy(region);
}

bool render_cpu_cover(app *application, surface *draw_surface, bool with_logo) {
    if (draw_surface->width == 0 || draw_surface->height == 0) {
        return false;
    }
    int scale = draw_surface->scale > 0 ? draw_surface->scale : 1;
    uint32_t width = draw_surface->width * (uint32_t)scale;
    uint32_t height = draw_surface->height * (uint32_t)scale;
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        log_error("invalid cover size %ux%u", width, height);
        return false;
    }
    size_t stride = (size_t)width * 4u;
    size_t size = stride * (size_t)height;

    int fd = create_shm_file(size);
    if (fd < 0) {
        log_error("cannot create shared memory: %s", strerror(errno));
        return false;
    }
    uint8_t *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        log_error("cannot map shared memory: %s", strerror(errno));
        close(fd);
        return false;
    }
    paint(&application->assets, (uint32_t *)data, width, height, with_logo);

    struct wl_shm_pool *pool =
        wl_shm_create_pool(application->shm, fd, (int32_t)size);
    if (!pool) {
        log_error("wl_shm_create_pool failed");
        munmap(data, size);
        close(fd);
        return false;
    }
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(
        pool, 0, (int32_t)width, (int32_t)height, (int32_t)stride,
        WL_SHM_FORMAT_XRGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    if (!buffer) {
        log_error("wl_shm_pool_create_buffer failed");
        munmap(data, size);
        return false;
    }

    Cover *cover = calloc(1, sizeof(*cover));
    if (!cover) {
        wl_buffer_destroy(buffer);
        munmap(data, size);
        return false;
    }
    cover->surface = draw_surface;
    cover->buffer = buffer;
    cover->data = data;
    cover->size = size;
    cover->width = width;
    cover->height = height;
    cover->stride = (uint32_t)stride;
    wl_buffer_add_listener(buffer, &cover_listener, cover);
    draw_surface->cover = cover;

    if (application->compositor_version >= 3) {
        wl_surface_set_buffer_scale(draw_surface->wl_surface, scale);
    }
    set_opaque_region(application, draw_surface);
    wl_surface_attach(draw_surface->wl_surface, buffer, 0, 0);
    wl_surface_damage(draw_surface->wl_surface, 0, 0,
                      (int32_t)draw_surface->width,
                      (int32_t)draw_surface->height);
    wl_surface_commit(draw_surface->wl_surface);
    wl_display_flush(application->display);
    return true;
}

void render_cpu_detach(surface *draw_surface) {
    Cover *cover = draw_surface->cover;
    if (!cover) {
        return;
    }
    cover->surface = NULL;
    draw_surface->cover = NULL;
}

void render_cpu_full(app *application, surface *draw_surface) {
    Cover *cover = draw_surface->cover;
    if (!cover) {
        render_cpu_cover(application, draw_surface, true);
        return;
    }
    paint(&application->assets, (uint32_t *)cover->data, cover->width,
          cover->height, true);
    wl_surface_attach(draw_surface->wl_surface, cover->buffer, 0, 0);
    wl_surface_damage(draw_surface->wl_surface, 0, 0,
                      (int32_t)draw_surface->width,
                      (int32_t)draw_surface->height);
    wl_surface_commit(draw_surface->wl_surface);
    wl_display_flush(application->display);
}
