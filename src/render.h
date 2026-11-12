#pragma once

#include <stdbool.h>

struct app;
struct surface;

bool render_init(struct app *app);
void render_surface_configure(struct app *app, struct surface *surface);
void render_surface_draw(struct app *app, struct surface *surface);
bool render_cpu_cover(struct app *app, struct surface *surface, bool with_logo);
void render_cpu_full(struct app *app, struct surface *surface);
void render_cpu_detach(struct surface *surface);
