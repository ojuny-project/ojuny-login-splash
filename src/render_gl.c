#include "render.h"

#include <math.h>
#include <string.h>

#include "app.h"
#include "util.h"

static void draw_quad(Gl *gl, GLuint texture, int view_width, int view_height,
                      double width, double height, double center_x,
                      double center_y, double angle, double scale, float alpha,
                      bool rotate) {
    double half_width = width * 0.5 * scale;
    double half_height = height * 0.5 * scale;
    double offsets[4][2] = {
        {-half_width, -half_height},
        {half_width, -half_height},
        {-half_width, half_height},
        {half_width, half_height},
    };
    float uv[4][2] = {
        {0.0f, 0.0f},
        {1.0f, 0.0f},
        {0.0f, 1.0f},
        {1.0f, 1.0f},
    };
    float vertices[4][4];
    double cosine = cos(angle);
    double sine = sin(angle);

    for (int index = 0; index < 4; ++index) {
        double x = offsets[index][0];
        double y = offsets[index][1];
        if (rotate) {
            double rotated_x = x * cosine - y * sine;
            double rotated_y = x * sine + y * cosine;
            x = rotated_x;
            y = rotated_y;
        }
        double screen_x = center_x + x;
        double screen_y = center_y + y;
        vertices[index][0] =
            (float)(2.0 * screen_x / (double)view_width - 1.0);
        vertices[index][1] =
            (float)(1.0 - 2.0 * screen_y / (double)view_height);
        vertices[index][2] = uv[index][0];
        vertices[index][3] = uv[index][1];
    }

    gl->p_glUseProgram(gl->program);
    gl->p_glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
    gl->p_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                       GL_DYNAMIC_DRAW);
    gl->p_glEnableVertexAttribArray((GLuint)gl->a_pos);
    gl->p_glEnableVertexAttribArray((GLuint)gl->a_uv);
    gl->p_glVertexAttribPointer((GLuint)gl->a_pos, 2, GL_FLOAT, GL_FALSE,
                                4 * sizeof(float), (const void *)0);
    gl->p_glVertexAttribPointer((GLuint)gl->a_uv, 2, GL_FLOAT, GL_FALSE,
                                4 * sizeof(float),
                                (const void *)(2 * sizeof(float)));
    gl->p_glBindTexture(GL_TEXTURE_2D, texture);
    gl->p_glUniform1f(gl->u_alpha, alpha);
    gl->p_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static void frame_done(void *data, struct wl_callback *callback,
                       uint32_t callback_time) {
    (void)callback_time;
    surface *draw_surface = data;
    wl_callback_destroy(callback);
    draw_surface->frame_cb = NULL;
    if (draw_surface->app->running && draw_surface->app->egl_ready) {
        render_surface_draw(draw_surface->app, draw_surface);
    }
}

static const struct wl_callback_listener frame_listener = {
    .done = frame_done,
};

void render_surface_draw(app *application, surface *draw_surface) {
    if (!application->egl_ready ||
        draw_surface->egl_surface == EGL_NO_SURFACE) {
        return;
    }
    Gl *gl = &application->gl;
    if (!gl->p_eglMakeCurrent(gl->display, draw_surface->egl_surface,
                              draw_surface->egl_surface, gl->context)) {
        log_error("eglMakeCurrent failed: 0x%x", gl->p_eglGetError());
        return;
    }

    int width = draw_surface->scale * (int)draw_surface->width;
    int height = draw_surface->scale * (int)draw_surface->height;
    if (width <= 0 || height <= 0) {
        return;
    }
    double now = now_monotonic();
    double exit_alpha = 1.0;
    if (application->have_stop_deadline && application->fade_out_started &&
        application->opts.fade_out > 0.0) {
        double remaining = application->stop_deadline - now;
        if (remaining < 0.0) {
            remaining = 0.0;
        }
        exit_alpha = clampd(remaining / application->opts.fade_out, 0.0, 1.0);
    }

    gl->p_glViewport(0, 0, width, height);
    float red = (float)((application->assets.bg_rgb >> 16) & 255u) / 255.0f;
    float green = (float)((application->assets.bg_rgb >> 8) & 255u) / 255.0f;
    float blue = (float)(application->assets.bg_rgb & 255u) / 255.0f;
    gl->p_glClearColor(red, green, blue, (float)exit_alpha);
    gl->p_glClear(GL_COLOR_BUFFER_BIT);

    AnimFrame frame = anim_sample(&application->anim, now);
    float content_alpha = (float)(frame.alpha * exit_alpha);
    double center_x = (double)width * 0.5;
    double center_y = (double)height * 0.5;
    draw_quad(gl, gl->logo_tex, width, height,
              (double)application->assets.logo_w,
              (double)application->assets.logo_h, center_x, center_y,
              frame.angle, frame.scale, content_alpha, true);

    double text_center_x = (double)width * 0.5;
    double text_center_y =
        (double)height * 0.5 +
        (double)application->assets.logo_content * 0.5 +
        (double)application->assets.text_gap +
        (double)application->assets.text_h * 0.5;
    draw_quad(gl, gl->text_tex, width, height,
              (double)application->assets.text_w,
              (double)application->assets.text_h, text_center_x, text_center_y,
              0.0, frame.scale, content_alpha, false);

    if (gl->tip_tex != 0) {
        double tip_top =
            (double)height * 0.5 +
            (double)application->assets.logo_content * 0.5 +
            (double)application->assets.text_gap +
            (double)application->assets.text_h +
            (double)application->assets.tip_gap;
        double tip_center_y =
            tip_top + (double)application->assets.tip_h * 0.5;
        draw_quad(gl, gl->tip_tex, width, height,
                  (double)application->assets.tip_w,
                  (double)application->assets.tip_h, text_center_x,
                  tip_center_y, 0.0, frame.scale, content_alpha, false);
    }

    if (!frame.done || application->fade_out_started) {
        draw_surface->frame_cb = wl_surface_frame(draw_surface->wl_surface);
        if (draw_surface->frame_cb) {
            wl_callback_add_listener(draw_surface->frame_cb, &frame_listener,
                                     draw_surface);
        }
    }

    wl_surface_damage(draw_surface->wl_surface, 0, 0,
                      (int32_t)draw_surface->width,
                      (int32_t)draw_surface->height);
    if (!gl->p_eglSwapBuffers(gl->display, draw_surface->egl_surface)) {
        log_error("eglSwapBuffers failed: 0x%x", gl->p_eglGetError());
        if (draw_surface->frame_cb) {
            wl_callback_destroy(draw_surface->frame_cb);
            draw_surface->frame_cb = NULL;
        }
    }
}

void render_surface_configure(app *application, surface *draw_surface) {
    if (!application->egl_ready || !draw_surface->configured) {
        return;
    }
    if (draw_surface->frame_cb) {
        wl_callback_destroy(draw_surface->frame_cb);
        draw_surface->frame_cb = NULL;
    }
    if (draw_surface->egl_surface != EGL_NO_SURFACE) {
        gl_surface_destroy(&application->gl, draw_surface->egl_window,
                           draw_surface->egl_surface);
        draw_surface->egl_window = NULL;
        draw_surface->egl_surface = EGL_NO_SURFACE;
    }

    int width = draw_surface->scale * (int)draw_surface->width;
    int height = draw_surface->scale * (int)draw_surface->height;
    if (width <= 0 || height <= 0) {
        return;
    }
    if (!gl_surface_create(&application->gl, draw_surface->wl_surface, width,
                           height, &draw_surface->egl_window,
                           &draw_surface->egl_surface)) {
        log_error("cannot create the EGL window");
        render_cpu_full(application, draw_surface);
        return;
    }
    if (application->compositor_version >= 3) {
        wl_surface_set_buffer_scale(draw_surface->wl_surface,
                                    draw_surface->scale);
    }
    wl_surface_set_opaque_region(draw_surface->wl_surface, NULL);
    render_surface_draw(application, draw_surface);
}

bool render_init(app *application) {
    if (!gl_load(&application->gl)) {
        return false;
    }
    if (!gl_init(&application->gl, application->display, &application->assets)) {
        gl_unload(&application->gl);
        return false;
    }

    application->egl_ready = true;
    anim_start(&application->anim, now_monotonic());
    for (surface *draw_surface = application->surfaces; draw_surface;
         draw_surface = draw_surface->next) {
        if (draw_surface->configured) {
            render_surface_configure(application, draw_surface);
        }
    }
    wl_display_flush(application->display);
    return true;
}
