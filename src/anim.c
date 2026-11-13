#include "anim.h"

#include <math.h>
#include <string.h>

#include "util.h"

#define OJUNY_PI 3.14159265358979323846
#define OJUNY_INTRO_SECONDS 0.3

void anim_init(Anim *anim, double fade_seconds) {
    memset(anim, 0, sizeof(*anim));
    anim->fade = fade_seconds;
}

void anim_start(Anim *anim, double now) {
    anim->start = now;
    anim->started = true;
}

AnimFrame anim_sample(const Anim *anim, double now) {
    AnimFrame frame;
    memset(&frame, 0, sizeof(frame));

    if (!anim->started) {
        frame.alpha = anim->fade > 0.0 ? 0.0 : 1.0;
        frame.scale = 1.0;
        return frame;
    }

    double elapsed = now - anim->start;
    if (elapsed < 0.0) {
        elapsed = 0.0;
    }

    if (anim->fade > 0.0) {
        double progress = elapsed / anim->fade;
        if (progress > 1.0) {
            progress = 1.0;
        }
        double eased = smoothstep(progress);
        frame.alpha = eased;
        frame.scale = 0.85 + 0.15 * eased;
        frame.done = progress >= 1.0;
        return frame;
    }

    const double step = OJUNY_PI / 3.0;
    double cycles = floor(elapsed);
    double phase = elapsed - cycles;
    double base = fmod(cycles * step, 2.0 * OJUNY_PI);

    if (phase < 0.7) {
        frame.angle = base + step * smoothstep(phase / 0.7);
    } else {
        frame.angle = base + step;
    }
    frame.alpha = smoothstep(elapsed / OJUNY_INTRO_SECONDS);
    frame.scale = 1.0;
    frame.done = false;
    return frame;
}
