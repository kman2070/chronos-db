#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ares_bft.h"

int main(void) {
    #define NUM_LOGS 50

    LogEntry *logs = malloc(sizeof(LogEntry) * NUM_LOGS);
    for (int i = 0; i < NUM_LOGS; i++) {
        logs[i].timestamp = 1000 + i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "log entry %d", i);
    }

    MerkleNode *real_root_node = build_merkle_tree(logs, NUM_LOGS);
    Block block = { .block_id = 1 };
    memcpy(block.merkle_root, real_root_node->hash, HASH_SIZE);

    /* Simulate a tampered log */
    LogEntry *tampered_logs = malloc(sizeof(LogEntry) * NUM_LOGS);
    memcpy(tampered_logs, logs, sizeof(LogEntry) * NUM_LOGS);
    snprintf(tampered_logs[10].log_data, sizeof(tampered_logs[10].log_data), "TAMPERED");
    MerkleNode *fake_root_node = build_merkle_tree(tampered_logs, NUM_LOGS);

    printf("=== Scenario: N=4, f=1 (quorum needs 2f+1=3 matching votes) ===\n");

    printf("\n--- Case A: all 4 nodes honest, all report the real root ---\n");
    uint8_t votes_a[4][HASH_SIZE];
    for (int i = 0; i < 4; i++) memcpy(votes_a[i], real_root_node->hash, HASH_SIZE);
    bool result_a = verify_block_with_votes(&block, votes_a, 4, 1);
    printf("Expected: PASS | Got: %s\n", result_a ? "PASS" : "FAIL");

    printf("\n--- Case B: 2 of 4 nodes report a TAMPERED root (only 2 honest votes) ---\n");
    uint8_t votes_b[4][HASH_SIZE];
    memcpy(votes_b[0], real_root_node->hash, HASH_SIZE);
    memcpy(votes_b[1], real_root_node->hash, HASH_SIZE);
    memcpy(votes_b[2], fake_root_node->hash, HASH_SIZE);
    memcpy(votes_b[3], fake_root_node->hash, HASH_SIZE);
    bool result_b = verify_block_with_votes(&block, votes_b, 4, 1);
    printf("Expected: FAIL (only 2 honest votes, need 3) | Got: %s\n", result_b ? "PASS" : "FAIL");

    printf("\n=== Compare against the OLD function (abstract math only) ===\n");
    printf("Old verify_bft_consensus() for the SAME N=4,f=1 case:\n");
    bool old_result = verify_bft_consensus(&block, 4, 1);
    printf("Old function says: %s -- but it never looked at the actual votes,\n", old_result ? "PASS" : "FAIL");
    printf("so it would say PASS even in Case B where the real votes show tampering.\n");
    printf("This is exactly the gap verify_block_with_votes fixes.\n");

    free(logs);
    free(tampered_logs);
    return 0;
}
