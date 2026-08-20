#define _POSIX_C_SOURCE 199309L

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "chronos.h"

#define NUM_THREADS 8
#define WRITES_PER_THREAD 25000
#define TOTAL_CAPACITY (NUM_THREADS * WRITES_PER_THREAD)

typedef struct {
    ChronosContext *ctx;
    uint32_t thread_id;
} ThreadWorkerArg;

void* worker_thread(void *arg) {
    ThreadWorkerArg *t_arg = (ThreadWorkerArg *)arg;
    ChronosContext *ctx = t_arg->ctx;
    uint32_t tid = t_arg->thread_id;

    char msg[64];
    for (int i = 0; i < WRITES_PER_THREAD; i++) {
        snprintf(msg, sizeof(msg), "Worker Thread %u Log Entry #%d", tid, i);
        
        if (tid % 2 == 0) {
            chronos_log_aegis_event(ctx, 1, msg);
        } else {
            chronos_log_vortx_event(ctx, 2, msg);
        }
    }

    pthread_exit(NULL);
}

int main() {
    const char *db_path = "chronos_concurrency_test.db";
    remove(db_path);

    printf("====================================================\n");
    printf(" STAGE 1: MULTI-THREADED CONCURRENCY STRESS TEST\n");
    printf("====================================================\n");
    printf("Threads: %d | Writes/Thread: %d | Expected Total: %d\n\n",
           NUM_THREADS, WRITES_PER_THREAD, TOTAL_CAPACITY);

    ChronosContext ctx;
    if (chronos_init(&ctx, db_path, TOTAL_CAPACITY) != 0) {
        fprintf(stderr, "Failed to initialize Chronos-DB\n");
        return 1;
    }

    // Start background flusher thread
    chronos_start_bg_sync(&ctx, 20);

    pthread_t threads[NUM_THREADS];
    ThreadWorkerArg args[NUM_THREADS];

    printf("1. Spawning %d parallel writer threads...\n", NUM_THREADS);
    for (uint32_t i = 0; i < NUM_THREADS; i++) {
        args[i].ctx = &ctx;
        args[i].thread_id = i;
        if (pthread_create(&threads[i], NULL, worker_thread, &args[i]) != 0) {
            fprintf(stderr, "Error creating thread %u\n", i);
            return 1;
        }
    }

    // Wait for all threads to finish concurrent writes
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("   All threads completed writing.\n\n");

    // 2. Atomicity Verification
    uint64_t final_head = ctx.header->head_index;
    printf("2. Verifying Atomic Counter Integrity:\n");
    printf("   Expected Head Index: %d\n", TOTAL_CAPACITY);
    printf("   Actual Head Index:   %lu\n", final_head);

    if (final_head == TOTAL_CAPACITY) {
        printf("   [PASS] Atomic increments was 100%% accurate across threads!\n\n");
    } else {
        printf("   [FAIL] Race condition detected! Expected %d but got %lu\n\n",
               TOTAL_CAPACITY, final_head);
    }

    // 3. Cryptographic Chain Integrity Check
    printf("3. Verifying Cryptographic SHA-256 Hash Chain Integrity...\n");
    if (chronos_verify_integrity(&ctx)) {
        printf("   [PASS] Cryptographic log integrity intact under high contention!\n\n");
    } else {
        printf("   [FAIL] Hash chain corrupted during concurrent writes!\n\n");
    }

    chronos_close(&ctx);
    printf("====================================================\n");
    printf(" CONCURRENCY STRESS TEST COMPLETED\n");
    printf("====================================================\n");

    return 0;
}