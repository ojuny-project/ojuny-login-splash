#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "anim.h"

#define TEST_PI 3.14159265358979323846

static void close_to(double actual, double expected, double tolerance) {
    if (fabs(actual - expected) > tolerance) {
        fprintf(stderr, "expected %.6f, got %.6f\n", expected, actual);
        assert(0);
    }
}

static void test_rotation(void) {
    Anim anim;
    anim_init(&anim, 0.0);
    anim_start(&anim, 100.0);

    AnimFrame frame = anim_sample(&anim, 100.0);
    close_to(frame.angle, 0.0, 1e-9);
    close_to(frame.alpha, 1.0, 1e-9);
    close_to(frame.scale, 1.0, 1e-9);
    assert(!frame.done);

    frame = anim_sample(&anim, 100.35);
    close_to(frame.angle, TEST_PI / 6.0, 1e-9);

    frame = anim_sample(&anim, 100.7);
    close_to(frame.angle, TEST_PI / 3.0, 1e-9);

    frame = anim_sample(&anim, 100.9);
    close_to(frame.angle, TEST_PI / 3.0, 1e-9);

    frame = anim_sample(&anim, 101.0);
    close_to(frame.angle, TEST_PI / 3.0, 1e-9);

    frame = anim_sample(&anim, 101.35);
    close_to(frame.angle, TEST_PI / 2.0, 1e-9);
}

static void test_fade(void) {
    Anim anim;
    anim_init(&anim, 2.0);
    anim_start(&anim, 50.0);

    AnimFrame frame = anim_sample(&anim, 50.0);
    close_to(frame.alpha, 0.0, 1e-9);
    close_to(frame.scale, 0.85, 1e-9);
    assert(!frame.done);

    frame = anim_sample(&anim, 51.0);
    close_to(frame.alpha, 0.5, 1e-9);
    close_to(frame.scale, 0.925, 1e-9);
    assert(!frame.done);

    frame = anim_sample(&anim, 52.0);
    close_to(frame.alpha, 1.0, 1e-9);
    close_to(frame.scale, 1.0, 1e-9);
    assert(frame.done);
}

int main(void) {
    test_rotation();
    test_fade();
    printf("anim tests passed\n");
    return 0;
}
