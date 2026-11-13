#pragma once

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    double seconds;
    double fade;
    double fade_in;
    double fade_out;
    bool quit;
    bool help;
    bool version;
} Options;

void options_init(Options *options);
bool options_parse(Options *options, int argc, char **argv);
void options_usage(FILE *out);
