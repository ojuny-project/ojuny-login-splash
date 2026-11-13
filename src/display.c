#include "display.h"

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "app.h"
#include "render.h"
#include "util.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

static volatile sig_atomic_t stop_signal;

static void handle_signal(int signal_number) {
    (void)signal_number;
    stop_signal = 1;
}

static void install_signal_handlers(void) {
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
}

static void surface_create(app *application, output *out);
static void surface_destroy(surface *draw_surface);
static void output_destroy(output *out);

static void surface_destroy(surface *draw_surface) {
    app *application = draw_surface->app;
    if (draw_surface->frame_cb) {
        wl_callback_destroy(draw_surface->frame_cb);
        draw_surface->frame_cb = NULL;
    }
    if (draw_surface->egl_surface != EGL_NO_SURFACE) {
        gl_surface_destroy(&application->gl, draw_surface->egl_window,
                           draw_surface->egl_surface);
        draw_surface->egl_surface = EGL_NO_SURFACE;
        draw_surface->egl_window = NULL;
    }
    render_cpu_detach(draw_surface);
    if (draw_surface->layer_surface) {
        zwlr_layer_surface_v1_destroy(draw_surface->layer_surface);
        draw_surface->layer_surface = NULL;
    }
    if (draw_surface->wl_surface) {
        wl_surface_destroy(draw_surface->wl_surface);
        draw_surface->wl_surface = NULL;
    }

    surface **link = &application->surfaces;
    while (*link && *link != draw_surface) {
        link = &(*link)->next;
    }
    if (*link == draw_surface) {
        *link = draw_surface->next;
    }
    application->surface_count--;
    if (draw_surface->configured) {
        application->configured_count--;
    }
    if (draw_surface->output &&
        draw_surface->output->surface == draw_surface) {
        draw_surface->output->surface = NULL;
    }
    free(draw_surface);
}

static void output_destroy(output *out) {
    app *application = out->app;
    if (out->surface) {
        surface_destroy(out->surface);
    }
    if (out->wl_output) {
        wl_output_destroy(out->wl_output);
    }
    output **link = &application->outputs;
    while (*link && *link != out) {
        link = &(*link)->next;
    }
    if (*link == out) {
        *link = out->next;
    }
    free(out);
}

static void layer_configure(void *data,
                            struct zwlr_layer_surface_v1 *layer_surface,
                            uint32_t serial, uint32_t width, uint32_t height) {
    surface *draw_surface = data;
    app *application = draw_surface->app;
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    if (width == 0 || height == 0) {
        return;
    }
    bool changed =
        draw_surface->width != width || draw_surface->height != height;
    draw_surface->width = width;
    draw_surface->height = height;
    if (!draw_surface->configured) {
        draw_surface->configured = true;
        application->configured_count++;
    }
    if (!changed && draw_surface->cover) {
        return;
    }
    if (application->egl_ready) {
        render_surface_configure(application, draw_surface);
    } else {
        render_cpu_cover(application, draw_surface, false);
    }
}

static void layer_closed(void *data,
                         struct zwlr_layer_surface_v1 *layer_surface) {
    (void)layer_surface;
    surface *draw_surface = data;
    app *application = draw_surface->app;
    surface_destroy(draw_surface);
    if (application->surface_count == 0) {
        application->running = false;
    }
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
    .configure = layer_configure,
    .closed = layer_closed,
};

static void surface_create(app *application, output *out) {
    surface *draw_surface = calloc(1, sizeof(*draw_surface));
    if (!draw_surface) {
        return;
    }
    draw_surface->app = application;
    draw_surface->output = out;
    draw_surface->scale =
        application->compositor_version >= 3 && out->scale > 0 ? out->scale : 1;
    draw_surface->egl_surface = EGL_NO_SURFACE;

    draw_surface->wl_surface =
        wl_compositor_create_surface(application->compositor);
    if (!draw_surface->wl_surface) {
        free(draw_surface);
        return;
    }
    draw_surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        application->layer_shell, draw_surface->wl_surface, out->wl_output,
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "ojuny-login-splash");
    if (!draw_surface->layer_surface) {
        wl_surface_destroy(draw_surface->wl_surface);
        free(draw_surface);
        return;
    }
    zwlr_layer_surface_v1_add_listener(draw_surface->layer_surface,
                                       &layer_surface_listener, draw_surface);
    zwlr_layer_surface_v1_set_anchor(
        draw_surface->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
    zwlr_layer_surface_v1_set_size(draw_surface->layer_surface, 0, 0);
    zwlr_layer_surface_v1_set_exclusive_zone(draw_surface->layer_surface, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        draw_surface->layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
    if (application->compositor_version >= 3) {
        wl_surface_set_buffer_scale(draw_surface->wl_surface,
                                    draw_surface->scale);
    }
    wl_surface_commit(draw_surface->wl_surface);

    out->surface = draw_surface;
    draw_surface->next = application->surfaces;
    application->surfaces = draw_surface;
    application->surface_count++;
}

static void output_geometry(void *data, struct wl_output *wl_output, int32_t x,
                            int32_t y, int32_t physical_width,
                            int32_t physical_height, int32_t subpixel,
                            const char *make, const char *model,
                            int32_t transform) {
    (void)data;
    (void)wl_output;
    (void)x;
    (void)y;
    (void)physical_width;
    (void)physical_height;
    (void)subpixel;
    (void)make;
    (void)model;
    (void)transform;
}

static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                        int32_t width, int32_t height, int32_t refresh) {
    (void)data;
    (void)wl_output;
    (void)flags;
    (void)width;
    (void)height;
    (void)refresh;
}

static void output_done(void *data, struct wl_output *wl_output) {
    (void)data;
    (void)wl_output;
}

static void output_scale(void *data, struct wl_output *wl_output,
                         int32_t factor) {
    (void)wl_output;
    output *out = data;
    if (factor <= 0) {
        factor = 1;
    }
    if (out->scale == factor) {
        return;
    }
    out->scale = factor;
    if (!out->surface) {
        return;
    }
    out->surface->scale =
        out->app->compositor_version >= 3 ? factor : 1;
    if (!out->surface->configured) {
        return;
    }
    if (out->app->egl_ready) {
        render_surface_configure(out->app, out->surface);
    } else {
        render_cpu_cover(out->app, out->surface, false);
    }
}

static void output_name(void *data, struct wl_output *wl_output,
                        const char *name) {
    (void)data;
    (void)wl_output;
    (void)name;
}

static void output_description(void *data, struct wl_output *wl_output,
                               const char *description) {
    (void)data;
    (void)wl_output;
    (void)description;
}

static const struct wl_output_listener output_listener = {
    .geometry = output_geometry,
    .mode = output_mode,
    .done = output_done,
    .scale = output_scale,
    .name = output_name,
    .description = output_description,
};

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard,
                            uint32_t format, int32_t fd, uint32_t size) {
    (void)data;
    (void)keyboard;
    (void)format;
    (void)size;
    if (fd >= 0) {
        close(fd);
    }
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface,
                           struct wl_array *keys) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
    (void)keys;
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard,
                           uint32_t serial, struct wl_surface *surface) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)surface;
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard,
                         uint32_t serial, uint32_t time, uint32_t key,
                         uint32_t state) {
    (void)keyboard;
    (void)serial;
    (void)time;
    app *application = data;
    if (state == WL_KEYBOARD_KEY_STATE_PRESSED && key == 1) {
        application->running = false;
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
                               uint32_t serial, uint32_t mods_depressed,
                               uint32_t mods_latched, uint32_t mods_locked,
                               uint32_t group) {
    (void)data;
    (void)keyboard;
    (void)serial;
    (void)mods_depressed;
    (void)mods_latched;
    (void)mods_locked;
    (void)group;
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
                                 int32_t rate, int32_t delay) {
    (void)data;
    (void)keyboard;
    (void)rate;
    (void)delay;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

static void seat_capabilities(void *data, struct wl_seat *seat,
                              uint32_t capabilities) {
    app *application = data;
    bool has_keyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
    if (has_keyboard && !application->keyboard) {
        application->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(application->keyboard, &keyboard_listener,
                                 application);
    } else if (!has_keyboard && application->keyboard) {
        wl_keyboard_destroy(application->keyboard);
        application->keyboard = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name,
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface,
                            uint32_t version) {
    app *application = data;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        uint32_t bind_version = version < 4 ? version : 4;
        application->compositor_version = bind_version;
        application->compositor = wl_registry_bind(
            registry, name, &wl_compositor_interface, bind_version);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        application->shm =
            wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        uint32_t bind_version = version < 4 ? version : 4;
        application->layer_shell = wl_registry_bind(
            registry, name, &zwlr_layer_shell_v1_interface, bind_version);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        if (application->seat) {
            return;
        }
        uint32_t bind_version = version < 7 ? version : 7;
        application->seat =
            wl_registry_bind(registry, name, &wl_seat_interface, bind_version);
        wl_seat_add_listener(application->seat, &seat_listener, application);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        uint32_t bind_version = version < 3 ? version : 3;
        output *out = calloc(1, sizeof(*out));
        if (!out) {
            return;
        }
        out->app = application;
        out->global_name = name;
        out->scale = 1;
        out->wl_output = wl_registry_bind(registry, name, &wl_output_interface,
                                          bind_version);
        wl_output_add_listener(out->wl_output, &output_listener, out);
        out->next = application->outputs;
        application->outputs = out;
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name) {
    (void)registry;
    app *application = data;
    output *out = application->outputs;
    while (out) {
        output *next = out->next;
        if (out->global_name == name) {
            output_destroy(out);
        }
        out = next;
    }
    if (application->surface_count == 0) {
        application->running = false;
    }
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

bool display_init(app *application) {
    install_signal_handlers();
    application->display = wl_display_connect(NULL);
    if (!application->display) {
        log_error("cannot connect to the Wayland display");
        return false;
    }
    application->registry = wl_display_get_registry(application->display);
    wl_registry_add_listener(application->registry, &registry_listener,
                             application);
    if (wl_display_roundtrip(application->display) < 0) {
        log_error("Wayland roundtrip failed");
        return false;
    }
    if (!application->compositor || !application->shm ||
        !application->layer_shell) {
        log_error("compositor does not provide wl_compositor, wl_shm, and "
                  "zwlr_layer_shell_v1");
        return false;
    }

    for (output *out = application->outputs; out; out = out->next) {
        if (!out->surface) {
            surface_create(application, out);
        }
    }
    if (application->surface_count == 0) {
        log_error("no outputs were found");
        return false;
    }

    if (wl_display_roundtrip(application->display) < 0) {
        log_error("Wayland roundtrip failed");
        return false;
    }
    wl_display_flush(application->display);

    double deadline = now_monotonic() + 1.0;
    while (application->configured_count < application->surface_count &&
           now_monotonic() < deadline) {
        struct pollfd descriptor = {
            .fd = wl_display_get_fd(application->display),
            .events = POLLIN,
        };
        int result = poll(&descriptor, 1, 1000);
        if (result <= 0) {
            break;
        }
        if (wl_display_dispatch(application->display) < 0) {
            log_error("Wayland dispatch failed");
            break;
        }
    }
    if (application->configured_count == 0) {
        log_error("no layer surface was configured");
        return false;
    }
    log_info("first shm frame on %d output(s) after %.1f ms",
             application->configured_count,
             (now_monotonic() - application->start_time) * 1000.0);
    return true;
}

void display_loop(app *application) {
    while (application->running && !stop_signal) {
        while (wl_display_prepare_read(application->display) != 0) {
            if (wl_display_dispatch_pending(application->display) < 0) {
                application->running = false;
                break;
            }
        }
        if (!application->running) {
            wl_display_cancel_read(application->display);
            break;
        }
        wl_display_flush(application->display);

        struct pollfd descriptors[2] = {
            {.fd = wl_display_get_fd(application->display), .events = POLLIN},
            {.fd = application->runtime.fd, .events = POLLIN},
        };
        int timeout = -1;
        if (application->have_stop_deadline) {
            double remaining =
                application->stop_deadline - now_monotonic();
            if (remaining <= 0.0) {
                application->running = false;
                wl_display_cancel_read(application->display);
                break;
            }
            timeout = (int)(remaining * 1000.0) + 1;
        }

        int result = poll(descriptors, 2, timeout);
        if (result < 0) {
            wl_display_cancel_read(application->display);
            if (errno == EINTR) {
                continue;
            }
            log_error("poll failed: %s", strerror(errno));
            application->running = false;
            break;
        }
        if (result == 0) {
            wl_display_cancel_read(application->display);
        } else {
            if (descriptors[0].revents & POLLIN) {
                if (wl_display_read_events(application->display) < 0) {
                    log_error("Wayland read failed");
                    application->running = false;
                    break;
                }
            } else {
                wl_display_cancel_read(application->display);
            }
            if (descriptors[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
                application->running = false;
                break;
            }
            if (wl_display_dispatch_pending(application->display) < 0) {
                log_error("Wayland dispatch failed");
                application->running = false;
                break;
            }
            if (descriptors[1].revents & POLLIN) {
                for (;;) {
                    int client =
                        accept4(application->runtime.fd, NULL, NULL, SOCK_CLOEXEC);
                    if (client < 0) {
                        break;
                    }
                    char command = 0;
                    ssize_t got = read(client, &command, 1);
                    close(client);
                    if (got > 0 && command == 'q') {
                        application->running = false;
                        break;
                    }
                }
            }
        }
        wl_display_flush(application->display);
        if (application->have_stop_deadline &&
            now_monotonic() >= application->stop_deadline) {
            application->running = false;
        }
    }
}
