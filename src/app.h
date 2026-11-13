#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "anim.h"
#include "args.h"
#include "assets.h"
#include "gl.h"
#include "runtime.h"

struct app;
struct output;
struct surface;
struct zwlr_layer_shell_v1;

typedef struct surface {
    struct app *app;
    struct output *output;
    struct wl_surface *wl_surface;
    struct zwlr_layer_surface_v1 *layer_surface;
    uint32_t width;
    uint32_t height;
    int scale;
    bool configured;
    struct wl_egl_window *egl_window;
    EGLSurface egl_surface;
    struct wl_callback *frame_cb;
    void *cover;
    struct surface *next;
} surface;

typedef struct output {
    struct app *app;
    struct wl_output *wl_output;
    uint32_t global_name;
    int scale;
    surface *surface;
    struct output *next;
} output;

typedef struct app {
    Options opts;
    Assets assets;
    Anim anim;
    Runtime runtime;
    Gl gl;
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    uint32_t compositor_version;
    struct wl_shm *shm;
    struct zwlr_layer_shell_v1 *layer_shell;
    struct wl_seat *seat;
    struct wl_keyboard *keyboard;
    output *outputs;
    surface *surfaces;
    int surface_count;
    int configured_count;
    bool egl_ready;
    bool running;
    bool have_stop_deadline;
    bool fade_out_started;
    double start_time;
    double stop_deadline;
} app;
