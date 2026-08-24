#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "ares_bft.h"

#define TEST_LOG_COUNT 10000

void run_throughput_test(void) {
    printf("\n=== TEST 1: High-Throughput Merkle Tree Stress Test ===\n");
    
    LogEntry *logs = malloc(sizeof(LogEntry) * TEST_LOG_COUNT);
    if (!logs) {
        printf("Memory allocation failed!\n");
        return;
    }

    for (int i = 0; i < TEST_LOG_COUNT; i++) {
        logs[i].timestamp = 1700000000 + i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "Execution Log Payload #%d", i);
    }

    clock_t start = clock();
    MerkleNode *root = build_merkle_tree(logs, TEST_LOG_COUNT);
    clock_t end = clock();

    double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
    double tps = (TEST_LOG_COUNT / elapsed_ms) * 1000.0;

    printf("Processed %d logs in %.2f ms\n", TEST_LOG_COUNT, elapsed_ms);
    printf("Merkle Commitment Speed: %.2f Logs/Sec\n", tps);

    free(logs);
}

void run_fault_tolerance_matrix(void) {
    printf("\n=== TEST 2: Byzantine Quorum Matrix Test (3f + 1 Rule) ===\n");

    struct {
        uint32_t total_nodes;
        uint32_t faulty_nodes;
        bool expected_pass;
    } scenarios[] = {
        {4, 1, true},   
        {4, 2, false},  
        {7, 2, true},   
        {10, 3, true},  
        {10, 4, false}  
    };

    Block mock_block = { .block_id = 999, .log_count = 100 };

    for (size_t i = 0; i < sizeof(scenarios)/sizeof(scenarios[0]); i++) {
        uint32_t N = scenarios[i].total_nodes;
        uint32_t f = scenarios[i].faulty_nodes;
        
        bool result = verify_bft_consensus(&mock_block, N, f);
        printf("Scenario [N=%u, f=%u] -> Expected: %s | Result: %s\n",
               N, f, 
               scenarios[i].expected_pass ? "PASS" : "FAIL",
               result == scenarios[i].expected_pass ? "SUCCESS" : "TEST FAILED");
    }
}

int main(void) {
    run_throughput_test();
    run_fault_tolerance_matrix();
    return 0;
}
