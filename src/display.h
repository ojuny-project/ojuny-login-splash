#pragma once

#include <stdbool.h>

struct app;

bool display_init(struct app *app);
void display_loop(struct app *app);
