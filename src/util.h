#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

double now_monotonic(void);
double clampd(double value, double low, double high);
double smoothstep(double value);
void log_info(const char *format, ...);
void log_error(const char *format, ...);
int create_shm_file(size_t size);
bool write_all(int fd, const void *data, size_t size);
uint32_t random_u32(void);
