#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "chronos.h"

#define CAPACITY 10000000UL
#define MAX_RESULTS 2000000

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

int main(void) {
    ChronosContext ctx;
    if (chronos_init(&ctx, "large_test.db", CAPACITY) != 0) {
        fprintf(stderr, "open failed\n");
        return 1;
    }

    uint64_t head = ctx.header->head_index;
    uint64_t cap = ctx.header->total_capacity;
    uint64_t first_ts = ctx.records[0].timestamp_ns;
    uint64_t last_slot = (head - 1) % cap;
    uint64_t last_ts = ctx.records[last_slot].timestamp_ns;

    printf("=== Dataset ===\n");
    printf("Total records   : %lu\n", head);
    printf("Time span       : %.3f sec\n\n", (last_ts - first_ts) / 1e9);

    ChronosRecord *results = malloc(sizeof(ChronosRecord) * MAX_RESULTS);

    {
        uint64_t start_ns = first_ts;
        uint64_t end_ns = first_ts + (uint64_t)(0.01 * (last_ts - first_ts));
        uint64_t t0 = now_ns();
        size_t found = chronos_query_time_range(&ctx, start_ns, end_ns, results, MAX_RESULTS);
        uint64_t t1 = now_ns();
        printf("--- Query 1: time range only, first 1%% of span ---\n");
        printf("Matched: %zu   Time: %.3f ms\n\n", found, (t1 - t0) / 1e6);
    }

    {
        uint64_t start_ns = first_ts;
        uint64_t end_ns = last_ts;
        uint64_t t0 = now_ns();
        size_t found = chronos_query_time_range(&ctx, start_ns, end_ns, results, MAX_RESULTS);
        uint64_t t_search = now_ns();

        size_t matched = 0;
        for (size_t i = 0; i < found; i++) {
            if (results[i].severity == 2 && results[i].module_id == 1) matched++;
        }
        uint64_t t_filter = now_ns();

        printf("--- Query 2: COMPLEX -- full range AND severity==2 AND module==1 ---\n");
        printf("Records returned by time-range step : %zu (capped at %d)\n", found, MAX_RESULTS);
        printf("Time-range fetch time                : %.3f ms\n", (t_search - t0) / 1e6);
        printf("Filter (severity+module) time        : %.3f ms\n", (t_filter - t_search) / 1e6);
        printf("Matched after filters                 : %zu\n", matched);
        printf("Total end-to-end                      : %.3f ms\n\n", (t_filter - t0) / 1e6);
    }

    {
        uint64_t mid = first_ts + (last_ts - first_ts) / 2;
        uint64_t start_ns = mid;
        uint64_t end_ns = mid + (uint64_t)(0.01 * (last_ts - first_ts));
        uint64_t t0 = now_ns();
        size_t found = chronos_query_time_range(&ctx, start_ns, end_ns, results, MAX_RESULTS);
        uint64_t t1 = now_ns();
        printf("--- Query 3: narrow window in the MIDDLE of dataset ---\n");
        printf("Matched: %zu   Time: %.3f ms\n\n", found, (t1 - t0) / 1e6);
    }

    {
        uint64_t t0 = now_ns();
        bool ok = chronos_verify_integrity(&ctx);
        uint64_t t1 = now_ns();
        printf("--- Query 4: full chronos_verify_integrity() over 10M records ---\n");
        printf("Result: %s   Time: %.3f sec\n\n", ok ? "OK" : "FAILED", (t1 - t0) / 1e9);
    }

    free(results);
    chronos_close(&ctx);
    return 0;
}
