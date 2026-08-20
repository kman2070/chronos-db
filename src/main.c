#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "chronos.h"
#include "election.h"

#define CAPACITY 100000

static volatile sig_atomic_t keep_running = 1;

static void handle_shutdown(int sig) {
    (void)sig;
    keep_running = 0;
}

int main() {
    signal(SIGTERM, handle_shutdown);
    signal(SIGINT, handle_shutdown);

    int lock_fd = try_become_active("election.lock");
    if (lock_fd < 0) {
        perror("election");
        return 1;
    }
    printf("I am now the ACTIVE instance (pid=%d)\n", getpid());
    fflush(stdout);

    const char *db_path = "chronos_telemetry.db";

    ChronosContext ctx;
    if (chronos_init(&ctx, db_path, CAPACITY) != 0) {
        fprintf(stderr, "Chronos-DB Initialization failed.\n");
        return 1;
    }

    chronos_start_bg_sync(&ctx, 50);
    printf("Chronos-DB running. Logging events every second. Ctrl+C to stop.\n");
    fflush(stdout);

    int i = 0;
    while (keep_running) {
        if (i % 2 == 0) {
            chronos_log_aegis_event(&ctx, 1, "Aegis-S2Q Token Pruned");
        } else {
            chronos_log_vortx_event(&ctx, 2, "VortxVM Lifecycle State Change");
        }
        i++;
        sleep(1);
    }

    printf("Shutting down cleanly (head_index=%lu)...\n", ctx.header->head_index);
    chronos_close(&ctx);
    release_active(lock_fd);
    return 0;
}
