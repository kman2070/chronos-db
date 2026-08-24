#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "ares_bft.h"

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

static long get_rss_kb(void) {
    FILE *f = fopen("/proc/self/status", "r");
    if (!f) return -1;
    char line[256];
    long rss = -1;
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line + 6, "%ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
}

static void test_large_scale(size_t n) {
    printf("\n=== TEST A: Large-scale Merkle build, %zu logs ===\n", n);

    LogEntry *logs = malloc(sizeof(LogEntry) * n);
    if (!logs) { printf("malloc failed for %zu logs\n", n); return; }

    for (size_t i = 0; i < n; i++) {
        logs[i].timestamp = 1700000000 + i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "stress log payload #%zu", i);
    }

    long rss_before = get_rss_kb();
    uint64_t t0 = now_ns();
    MerkleNode *root = build_merkle_tree(logs, n);
    uint64_t t1 = now_ns();
    long rss_after = get_rss_kb();

    double ms = (t1 - t0) / 1e6;
    printf("Built tree from %zu logs in %.2f ms (%.0f logs/sec)\n", n, ms, n / (ms / 1000.0));
    printf("RSS before: %ld KB | RSS after: %ld KB | delta: %ld KB\n",
           rss_before, rss_after, rss_after - rss_before);
    printf("Root hash (first 8 bytes): ");
    for (int i = 0; i < 8; i++) printf("%02x", root->hash[i]);
    printf("...\n");

    free(logs);
}

static void test_edge_counts(void) {
    printf("\n=== TEST B: Edge-case log counts (1, 2, 3, 7) ===\n");
    size_t counts[] = {1, 2, 3, 7};
    for (size_t c = 0; c < sizeof(counts)/sizeof(counts[0]); c++) {
        size_t n = counts[c];
        LogEntry *logs = malloc(sizeof(LogEntry) * n);
        for (size_t i = 0; i < n; i++) {
            logs[i].timestamp = i;
            snprintf(logs[i].log_data, sizeof(logs[i].log_data), "edge log %zu", i);
        }
        MerkleNode *root = build_merkle_tree(logs, n);
        printf("count=%zu -> root built: %s\n", n, root ? "OK" : "NULL (unexpected!)");
        free(logs);
    }
}

static void test_vote_boundaries(void) {
    printf("\n=== TEST C: Exact boundary vote counts (2f+1 vs 2f) ===\n");

    LogEntry logs[10];
    for (int i = 0; i < 10; i++) {
        logs[i].timestamp = i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "boundary log %d", i);
    }
    MerkleNode *real_root = build_merkle_tree(logs, 10);
    Block block = { .block_id = 42 };
    memcpy(block.merkle_root, real_root->hash, HASH_SIZE);

    uint8_t fake_hash[HASH_SIZE];
    memset(fake_hash, 0x99, HASH_SIZE);

    struct { uint32_t N, f; } cases[] = { {4,1}, {7,2}, {10,3}, {13,4} };

    for (size_t c = 0; c < sizeof(cases)/sizeof(cases[0]); c++) {
        uint32_t N = cases[c].N, f = cases[c].f;
        uint32_t required = 2 * f + 1;

        uint8_t (*votes)[HASH_SIZE] = malloc(sizeof(uint8_t[HASH_SIZE]) * N);

        for (uint32_t i = 0; i < N; i++) {
            if (i < required) memcpy(votes[i], real_root->hash, HASH_SIZE);
            else memcpy(votes[i], fake_hash, HASH_SIZE);
        }
        bool pass_result = verify_block_with_votes(&block, votes, N, f);
        printf("N=%u f=%u: exactly %u honest votes -> %s (expected PASS)\n",
               N, f, required, pass_result ? "PASS" : "FAIL");

        for (uint32_t i = 0; i < N; i++) {
            if (i < required - 1) memcpy(votes[i], real_root->hash, HASH_SIZE);
            else memcpy(votes[i], fake_hash, HASH_SIZE);
        }
        bool fail_result = verify_block_with_votes(&block, votes, N, f);
        printf("N=%u f=%u: only %u honest votes  -> %s (expected FAIL)\n",
               N, f, required - 1, fail_result ? "PASS" : "FAIL");

        if (!pass_result || fail_result) {
            printf("  *** BOUNDARY TEST FAILED for N=%u f=%u ***\n", N, f);
        }

        free(votes);
    }
}

static void test_large_matrix(void) {
    printf("\n=== TEST D: Large N adversarial matrix (N up to 200) ===\n");

    LogEntry logs[5];
    for (int i = 0; i < 5; i++) {
        logs[i].timestamp = i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "matrix log %d", i);
    }
    MerkleNode *real_root = build_merkle_tree(logs, 5);
    Block block = { .block_id = 7 };
    memcpy(block.merkle_root, real_root->hash, HASH_SIZE);
    uint8_t fake_hash[HASH_SIZE];
    memset(fake_hash, 0x55, HASH_SIZE);

    uint32_t Ns[] = {13, 31, 100, 200};
    int mismatches = 0;

    for (size_t ni = 0; ni < sizeof(Ns)/sizeof(Ns[0]); ni++) {
        uint32_t N = Ns[ni];
        uint32_t f_max = (N - 1) / 3;

        uint8_t (*votes)[HASH_SIZE] = malloc(sizeof(uint8_t[HASH_SIZE]) * N);

        for (uint32_t i = 0; i < N; i++) {
            if (i < f_max) memcpy(votes[i], fake_hash, HASH_SIZE);
            else memcpy(votes[i], real_root->hash, HASH_SIZE);
        }
        bool result = verify_block_with_votes(&block, votes, N, f_max);
        printf("N=%u f_max=%u (worst-case faulty count) -> %s (expected PASS)\n",
               N, f_max, result ? "PASS" : "FAIL");
        if (!result) mismatches++;

        free(votes);
    }
    printf("Mismatches: %d\n", mismatches);
}

static void test_memory_growth(int iterations, size_t logs_per_iter) {
    printf("\n=== TEST E: Repeated tree builds (%d x %zu logs) -- memory growth check ===\n",
           iterations, logs_per_iter);
    printf("(build_merkle_tree currently never frees its nodes -- this will show it)\n\n");

    LogEntry *logs = malloc(sizeof(LogEntry) * logs_per_iter);
    for (size_t i = 0; i < logs_per_iter; i++) {
        logs[i].timestamp = i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "growth log %zu", i);
    }

    long rss_start = get_rss_kb();
    printf("RSS at start: %ld KB\n", rss_start);

    for (int i = 0; i < iterations; i++) {
        MerkleNode *root = build_merkle_tree(logs, logs_per_iter);
        free_merkle_tree(root);
        if ((i + 1) % (iterations / 5 == 0 ? 1 : iterations / 5) == 0 || i == iterations - 1) {
            long rss_now = get_rss_kb();
            printf("After %4d builds: RSS = %6ld KB (+%ld KB since start)\n",
                   i + 1, rss_now, rss_now - rss_start);
        }
    }

    free(logs);
}

int main(void) {
    test_large_scale(500000);
    test_edge_counts();
    test_vote_boundaries();
    test_large_matrix();
    test_memory_growth(50, 10000);

    printf("\n=== ALL HARD TESTS COMPLETE ===\n");
    return 0;
}
