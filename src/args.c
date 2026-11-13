#include "args.h"

#include <errno.h>
#include <getopt.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool parse_number(const char *text, double *out, bool allow_zero) {
    char *end = NULL;
    errno = 0;
    double value = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(value)) {
        return false;
    }
    if (value < 0.0 || (!allow_zero && value == 0.0)) {
        return false;
    }
    *out = value;
    return true;
}

void options_init(Options *options) {
    memset(options, 0, sizeof(*options));
    options->seconds = -1.0;
    options->fade = 0.0;
    options->fade_in = 0.3;
    options->fade_out = 0.3;
}

bool options_parse(Options *options, int argc, char **argv) {
    static const struct option long_options[] = {
        {"seconds", required_argument, NULL, 's'},
        {"fade", required_argument, NULL, 'f'},
        {"fade-in", required_argument, NULL, 'i'},
        {"fade-out", required_argument, NULL, 'o'},
        {"quit", no_argument, NULL, 'q'},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {NULL, 0, NULL, 0},
    };

    optind = 1;
    opterr = 0;
    int option = 0;
    while ((option = getopt_long(argc, argv, "", long_options, NULL)) != -1) {
        switch (option) {
        case 's':
            if (!parse_number(optarg, &options->seconds, true)) {
                return false;
            }
            break;
        case 'f':
            if (!parse_number(optarg, &options->fade, false)) {
                return false;
            }
            break;
        case 'i':
            if (!parse_number(optarg, &options->fade_in, true)) {
                return false;
            }
            break;
        case 'o':
            if (!parse_number(optarg, &options->fade_out, true)) {
                return false;
            }
            break;
        case 'q':
            options->quit = true;
            break;
        case 'h':
            options->help = true;
            break;
        case 'v':
            options->version = true;
            break;
        default:
            return false;
        }
    }
    return optind == argc;
}

void options_usage(FILE *out) {
    fprintf(out,
            "usage: ojuny-login-splash [options]\n"
            "\n"
            "  --seconds N   exit after N seconds\n"
            "  --fade N      fade and grow over N seconds instead of rotating\n"
            "  --fade-in N   asset fade-in over N seconds (default 0.3)\n"
            "  --fade-out N  fade out over N seconds before the timer exits (default 0.3)\n"
            "  --quit        stop all running instances\n"
            "  --help        show this help\n"
            "  --version     show the version\n");
}
