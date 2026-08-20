#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "chronos.h"

#define TOTAL_RECORDS 10000000UL
#define NUM_THREADS 4

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

typedef struct {
    ChronosContext *ctx;
    unsigned long count;
    int thread_id;
} SeedArg;

static void* seed_worker(void *arg) {
    SeedArg *a = (SeedArg*)arg;
    char payload[64];
    for (unsigned long i = 0; i < a->count; i++) {
        uint32_t severity = (a->thread_id + i) % 4;
        uint32_t module = (i % 2 == 0) ? 1 : 2;
        snprintf(payload, sizeof(payload), "seed record thread=%d i=%lu", a->thread_id, i);
        chronos_append(a->ctx, module, severity, (const uint8_t*)payload, strlen(payload));
    }
    return NULL;
}

int main(void) {
    printf("=== Seeding %lu records into large_test.db ===\n", TOTAL_RECORDS);

    ChronosContext ctx;
    uint64_t t0 = now_ns();
    if (chronos_init(&ctx, "large_test.db", TOTAL_RECORDS) != 0) {
        fprintf(stderr, "init failed\n");
        return 1;
    }
    uint64_t t1 = now_ns();
    printf("chronos_init (fallocate + mmap %lu records): %.3f sec\n",
           TOTAL_RECORDS, (t1 - t0) / 1e9);

    pthread_t threads[NUM_THREADS];
    SeedArg args[NUM_THREADS];
    unsigned long per_thread = TOTAL_RECORDS / NUM_THREADS;

    uint64_t t2 = now_ns();
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].ctx = &ctx;
        args[i].count = per_thread;
        args[i].thread_id = i;
        pthread_create(&threads[i], NULL, seed_worker, &args[i]);
    }
    for (int i = 0; i < NUM_THREADS; i++) pthread_join(threads[i], NULL);
    uint64_t t3 = now_ns();

    double insert_sec = (t3 - t2) / 1e9;
    printf("Inserted %lu records in %.3f sec  (%.0f ops/sec)\n",
           TOTAL_RECORDS, insert_sec, TOTAL_RECORDS / insert_sec);

    chronos_flush(&ctx, true);
    uint64_t t4 = now_ns();
    printf("Final msync (MS_SYNC): %.3f sec\n", (t4 - t3) / 1e9);

    printf("head_index = %lu\n", ctx.header->head_index);

    chronos_close(&ctx);
    return 0;
}
