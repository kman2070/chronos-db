#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "chronos.h"

#define DB_CAPACITY 100000

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s [--db PATH] --last SECONDS [--severity N] [--module N]\n"
        "\n"
        "Examples:\n"
        "  %s --last 60                 # everything from the last 60 seconds\n"
        "  %s --last 300 --severity 2   # last 5 minutes, only severity==2\n"
        "  %s --db /var/lib/chronos/chronos_telemetry.db --last 30\n",
        prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    const char *db_path = "/var/lib/chronos/chronos_telemetry.db";
    long last_seconds = -1;
    int filter_severity = -1;
    int filter_module = -1;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--last") == 0 && i + 1 < argc) {
            last_seconds = atol(argv[++i]);
        } else if (strcmp(argv[i], "--severity") == 0 && i + 1 < argc) {
            filter_severity = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--module") == 0 && i + 1 < argc) {
            filter_module = atoi(argv[++i]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (last_seconds <= 0) {
        print_usage(argv[0]);
        return 1;
    }

    ChronosContext ctx;
    if (chronos_init(&ctx, db_path, DB_CAPACITY) != 0) {
        fprintf(stderr, "Could not open %s (are you running as the right user? try sudo)\n", db_path);
        return 1;
    }

    uint64_t end_ns = now_ns();
    uint64_t start_ns = end_ns - ((uint64_t)last_seconds * 1000000000ULL);

    #define MAX_RESULTS 2000
    ChronosRecord *results = malloc(sizeof(ChronosRecord) * MAX_RESULTS);
    size_t found = chronos_query_time_range(&ctx, start_ns, end_ns, results, MAX_RESULTS);

    size_t shown = 0;
    for (size_t i = 0; i < found; i++) {
        if (filter_severity >= 0 && results[i].severity != (uint32_t)filter_severity) continue;
        if (filter_module >= 0 && results[i].module_id != (uint32_t)filter_module) continue;

        time_t sec = results[i].timestamp_ns / 1000000000ULL;
        char timebuf[32];
        strftime(timebuf, sizeof(timebuf), "%H:%M:%S", localtime(&sec));

        printf("[%s] module=%u severity=%u payload=\"%.80s\"\n",
               timebuf, results[i].module_id, results[i].severity, results[i].payload);
        shown++;
    }

    printf("\n%zu record(s) matched (out of %zu in that time range).\n", shown, found);

    free(results);
    chronos_close(&ctx);
    return 0;
}
