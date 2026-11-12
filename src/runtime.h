#pragma once

#include <stdbool.h>

typedef struct Runtime {
    int fd;
    bool ready;
    char dir[512];
    char path[512];
} Runtime;

bool runtime_init(Runtime *runtime);
void runtime_destroy(Runtime *runtime);
int runtime_quit_all(void);
