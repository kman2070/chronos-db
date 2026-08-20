#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include "election.h"

int try_become_active(const char *lock_path) {
    int fd = open(lock_path, O_CREAT | O_RDWR, 0600);
    if (fd < 0) return -1;

    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void release_active(int lock_fd) {
    if (lock_fd >= 0) {
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
    }
}
