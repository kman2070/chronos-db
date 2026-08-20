#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include "chronos.h"

#define NUM_RECORDS 100000
#define NUM_THREADS 4

static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

typedef struct {
    ChronosContext *ctx;
    size_t count;
} ThreadArg;

static void* worker_benchmark(void *arg) {
    ThreadArg *targ = (ThreadArg *)arg;
    uint8_t dummy_payload[16] = "bench_test_data";

    for (size_t i = 0; i < targ->count; i++) {
        chronos_append(targ->ctx, 1, 1, dummy_payload, sizeof(dummy_payload));
    }
    pthread_exit(NULL);
}

int main(void) {
    printf("=== Starting Chronos Benchmark ===\n");

    ChronosContext ctx;
    if (chronos_init(&ctx, "benchmark.db", NUM_RECORDS) != 0) {
        printf("Failed to initialize database.\n");
        return 1;
    }

    uint64_t start_time = get_time_ns();

    pthread_t threads[NUM_THREADS];
    ThreadArg args[NUM_THREADS];
    size_t records_per_thread = NUM_RECORDS / NUM_THREADS;

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].ctx = &ctx;
        args[i].count = records_per_thread;
        pthread_create(&threads[i], NULL, worker_benchmark, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    uint64_t end_time = get_time_ns();
    double total_sec = (double)(end_time - start_time) / 1e9;
    double avg_latency_ns = (double)(end_time - start_time) / NUM_RECORDS;
    double ops = (double)NUM_RECORDS / total_sec;

    printf("\n[Append Performance]\n");
    printf("Records Processed : %d\n", NUM_RECORDS);
    printf("Total Time        : %.4f sec\n", total_sec);
    printf("Avg Append Latency: %.2f ns (%.4f ms)\n", avg_latency_ns, avg_latency_ns / 1e6);
    printf("Throughput        : %.2f OPS\n", ops);

    uint64_t query_start = get_time_ns();
    ChronosRecord out_buffer[100];
    size_t found = chronos_query_time_range(&ctx, start_time, end_time, out_buffer, 100);
    uint64_t query_end = get_time_ns();

    double query_latency_us = (double)(query_end - query_start) / 1000.0;

    printf("\n[Binary Search Query Performance]\n");
    printf("Records Fetched   : %zu\n", found);
    printf("Query Latency     : %.3f us (%.5f ms)\n", query_latency_us, query_latency_us / 1000.0);

    chronos_close(&ctx);
    remove("benchmark.db");

    printf("\n=== Benchmark Complete ===\n");
    return 0;
}