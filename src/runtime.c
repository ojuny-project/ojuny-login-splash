#include "runtime.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "util.h"

static bool runtime_dir(char *out, size_t out_size) {
    const char *base = getenv("XDG_RUNTIME_DIR");
    int written;
    if (base && *base) {
        written = snprintf(out, out_size, "%s/ojuny-login-splash", base);
    } else {
        written = snprintf(out, out_size, "/tmp/ojuny-login-splash-%lu",
                           (unsigned long)getuid());
    }
    return written > 0 && (size_t)written < out_size;
}

static bool make_runtime_dir(const char *dir) {
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        log_error("cannot create %s: %s", dir, strerror(errno));
        return false;
    }
    struct stat stat_buffer;
    if (stat(dir, &stat_buffer) != 0 || !S_ISDIR(stat_buffer.st_mode) ||
        stat_buffer.st_uid != getuid()) {
        log_error("unsafe runtime directory: %s", dir);
        return false;
    }
    return true;
}

static bool fill_address(struct sockaddr_un *address, const char *path) {
    memset(address, 0, sizeof(*address));
    address->sun_family = AF_UNIX;
    size_t length = strlen(path);
    if (length >= sizeof(address->sun_path)) {
        log_error("socket path is too long: %s", path);
        return false;
    }
    memcpy(address->sun_path, path, length + 1);
    return true;
}

bool runtime_init(Runtime *runtime) {
    memset(runtime, 0, sizeof(*runtime));
    runtime->fd = -1;

    if (!runtime_dir(runtime->dir, sizeof(runtime->dir))) {
        log_error("runtime directory path is too long");
        return false;
    }
    if (!make_runtime_dir(runtime->dir)) {
        return false;
    }
    int written = snprintf(runtime->path, sizeof(runtime->path), "%s/%ld.sock",
                           runtime->dir, (long)getpid());
    if (written <= 0 || (size_t)written >= sizeof(runtime->path)) {
        log_error("socket path is too long");
        return false;
    }

    struct sockaddr_un address;
    if (!fill_address(&address, runtime->path)) {
        return false;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        log_error("cannot create socket: %s", strerror(errno));
        return false;
    }
    unlink(runtime->path);
    if (bind(fd, (struct sockaddr *)&address, sizeof(address)) != 0) {
        log_error("cannot bind %s: %s", runtime->path, strerror(errno));
        close(fd);
        return false;
    }
    chmod(runtime->path, 0600);
    if (listen(fd, 8) != 0) {
        log_error("cannot listen on %s: %s", runtime->path, strerror(errno));
        close(fd);
        unlink(runtime->path);
        return false;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
    runtime->fd = fd;
    runtime->ready = true;
    return true;
}

void runtime_destroy(Runtime *runtime) {
    if (runtime->ready) {
        unlink(runtime->path);
        close(runtime->fd);
        runtime->fd = -1;
        runtime->ready = false;
    }
}

int runtime_quit_all(void) {
    char dir[512];
    if (!runtime_dir(dir, sizeof(dir))) {
        log_error("runtime directory path is too long");
        return -1;
    }

    DIR *directory = opendir(dir);
    if (!directory) {
        if (errno == ENOENT) {
            return 0;
        }
        log_error("cannot open %s: %s", dir, strerror(errno));
        return -1;
    }

    int signaled = 0;
    struct dirent *entry;
    while ((entry = readdir(directory)) != NULL) {
        size_t length = strlen(entry->d_name);
        if (length < 5 || strcmp(entry->d_name + length - 5, ".sock") != 0) {
            continue;
        }
        char path[512];
        int written = snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(path)) {
            continue;
        }
        struct sockaddr_un address;
        if (!fill_address(&address, path)) {
            continue;
        }
        int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            continue;
        }
        if (connect(fd, (struct sockaddr *)&address, sizeof(address)) == 0) {
            if (write_all(fd, "q", 1)) {
                ++signaled;
            }
            close(fd);
        } else {
            close(fd);
            unlink(path);
        }
    }
    closedir(directory);
    return signaled;
}
