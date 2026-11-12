#include "util.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

double now_monotonic(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0.0;
    }
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
}

double clampd(double value, double low, double high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

double smoothstep(double value) {
    value = clampd(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

static void log_message(const char *level, const char *format, va_list args) {
    fprintf(stderr, "[ojuny-splash] %s", level);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("", format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("error: ", format, args);
    va_end(args);
}

int create_shm_file(size_t size) {
#ifdef __linux__
    int fd = memfd_create("ojuny-login-splash", MFD_CLOEXEC);
    if (fd >= 0) {
        if (ftruncate(fd, (off_t)size) == 0) {
            return fd;
        }
        close(fd);
    }
#endif

    for (int attempt = 0; attempt < 100; ++attempt) {
        char name[96];
        snprintf(name, sizeof(name), "/ojuny-splash-%ld-%d", (long)getpid(), attempt);
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            if (errno == EEXIST) {
                continue;
            }
            return -1;
        }
        shm_unlink(name);
        if (ftruncate(fd, (off_t)size) == 0) {
            return fd;
        }
        close(fd);
        return -1;
    }
    return -1;
}

bool write_all(int fd, const void *data, size_t size) {
    const unsigned char *cursor = data;
    while (size > 0) {
        ssize_t written = write(fd, cursor, size);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        cursor += (size_t)written;
        size -= (size_t)written;
    }
    return true;
}
