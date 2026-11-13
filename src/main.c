#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "assets.h"
#include "display.h"
#include "render.h"
#include "runtime.h"
#include "util.h"

#ifndef VERSION
#define VERSION "0.1.0"
#endif

int main(int argc, char **argv) {
    app application;
    memset(&application, 0, sizeof(application));
    application.start_time = now_monotonic();

    options_init(&application.opts);
    if (!options_parse(&application.opts, argc, argv)) {
        options_usage(stderr);
        return 2;
    }
    if (application.opts.help) {
        options_usage(stdout);
        return 0;
    }
    if (application.opts.version) {
        printf("ojuny-login-splash %s\n", VERSION);
        return 0;
    }
    if (application.opts.quit) {
        int signaled = runtime_quit_all();
        return signaled >= 0 ? 0 : 1;
    }

    if (!assets_load(&application.assets)) {
        return 1;
    }
    anim_init(&application.anim, application.opts.fade);
    if (!runtime_init(&application.runtime)) {
        assets_unload(&application.assets);
        return 1;
    }

    application.running = true;
    if (application.opts.seconds >= 0.0) {
        application.have_stop_deadline = true;
        application.stop_deadline =
            application.start_time + application.opts.seconds;
    }

    bool started = display_init(&application);
    if (started) {
        if (render_init(&application)) {
            log_info("first EGL frame after %.1f ms",
                     (now_monotonic() - application.start_time) * 1000.0);
        } else {
            log_info("EGL unavailable, keeping the static shm frame");
            for (surface *draw_surface = application.surfaces; draw_surface;
                 draw_surface = draw_surface->next) {
                if (draw_surface->configured) {
                    render_cpu_full(&application, draw_surface);
                }
            }
        }
        display_loop(&application);
    }

    runtime_destroy(&application.runtime);
    assets_unload(&application.assets);
    return started ? 0 : 1;
}
