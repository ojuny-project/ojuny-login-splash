#pragma once

#include <stdbool.h>

typedef struct {
    double start;
    double fade;
    double fade_in;
    bool started;
} Anim;

typedef struct {
    double angle;
    double alpha;
    double scale;
    bool done;
} AnimFrame;

void anim_init(Anim *anim, double fade_seconds, double fade_in_seconds);
void anim_start(Anim *anim, double now);
AnimFrame anim_sample(const Anim *anim, double now);
