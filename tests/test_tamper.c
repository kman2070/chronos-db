#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ares_bft.h"

static void print_hash(const char *label, uint8_t *hash) {
    printf("%s: ", label);
    for (int i = 0; i < 8; i++) printf("%02x", hash[i]);
    printf("...\n");
}

int main(void) {
    #define NUM_LOGS 100

    LogEntry *logs = malloc(sizeof(LogEntry) * NUM_LOGS);
    for (int i = 0; i < NUM_LOGS; i++) {
        logs[i].timestamp = 1000 + i;
        snprintf(logs[i].log_data, sizeof(logs[i].log_data), "log entry number %d - normal data", i);
    }

    printf("=== Building Merkle tree from %d untampered logs ===\n", NUM_LOGS);
    MerkleNode *root1 = build_merkle_tree(logs, NUM_LOGS);
    print_hash("Original root hash", root1->hash);

    printf("\n=== Tampering with log entry #42 (changing its content) ===\n");
    printf("Before: \"%s\"\n", logs[42].log_data);
    snprintf(logs[42].log_data, sizeof(logs[42].log_data), "log entry number 42 - TAMPERED DATA");
    printf("After:  \"%s\"\n", logs[42].log_data);

    printf("\n=== Rebuilding Merkle tree after tampering ===\n");
    MerkleNode *root2 = build_merkle_tree(logs, NUM_LOGS);
    print_hash("New root hash", root2->hash);

    printf("\n=== Result ===\n");
    if (memcmp(root1->hash, root2->hash, HASH_SIZE) != 0) {
        printf("PASS: root hash changed -- tampering was detected.\n");
    } else {
        printf("FAIL: root hash is IDENTICAL -- tampering was NOT detected!\n");
        printf("      (this would mean compute_sha256 is still a stub, or logic is broken)\n");
    }

    free(logs);
    return 0;
}
