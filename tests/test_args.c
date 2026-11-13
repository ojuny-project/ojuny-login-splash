#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "args.h"

static void parse(Options *options, int argc, char **argv) {
    options_init(options);
    assert(options_parse(options, argc, argv));
}

int main(void) {
    Options options;

    {
        char *argv[] = {"splash", "--seconds", "1.5", "--fade", "2", NULL};
        parse(&options, 5, argv);
        assert(fabs(options.seconds - 1.5) < 1e-9);
        assert(fabs(options.fade - 2.0) < 1e-9);
        assert(fabs(options.fade_in - 0.3) < 1e-9);
        assert(fabs(options.fade_out - 0.3) < 1e-9);
        assert(!options.quit);
    }
    {
        char *argv[] = {"splash", "--fade-in", "0", "--fade-out", "0.5", NULL};
        parse(&options, 5, argv);
        assert(options.fade_in == 0.0);
        assert(fabs(options.fade_out - 0.5) < 1e-9);
    }
    {
        char *argv[] = {"splash", "--quit", NULL};
        parse(&options, 2, argv);
        assert(options.quit);
        assert(options.seconds < 0.0);
    }
    {
        char *argv[] = {"splash", "--help", NULL};
        parse(&options, 2, argv);
        assert(options.help);
    }
    {
        char *argv[] = {"splash", "--seconds", "0", NULL};
        parse(&options, 3, argv);
        assert(options.seconds == 0.0);
    }
    {
        char *argv[] = {"splash", "--fade", "0", NULL};
        options_init(&options);
        assert(!options_parse(&options, 3, argv));
    }
    {
        char *argv[] = {"splash", "--unknown", NULL};
        options_init(&options);
        assert(!options_parse(&options, 2, argv));
    }
    {
        char *argv[] = {"splash", "--seconds", NULL};
        options_init(&options);
        assert(!options_parse(&options, 2, argv));
    }

    printf("args tests passed\n");
    return 0;
}
