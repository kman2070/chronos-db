#ifndef ARES_BFT_H
#define ARES_BFT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define HASH_SIZE 32

typedef struct LogEntry {
    uint64_t timestamp;
    char log_data[256];
    uint8_t hash[HASH_SIZE];
} LogEntry;

typedef struct MerkleNode {
    uint8_t hash[HASH_SIZE];
    struct MerkleNode *left;
    struct MerkleNode *right;
} MerkleNode;

typedef struct Block {
    uint32_t block_id;
    uint8_t prev_block_hash[HASH_SIZE];
    uint8_t merkle_root[HASH_SIZE];
    uint32_t log_count;
    LogEntry *logs;
} Block;

void compute_sha256(const unsigned char *data, size_t len, uint8_t *output);
MerkleNode* build_merkle_tree(LogEntry *logs, size_t count);

/* Frees every node in a tree returned by build_merkle_tree. Handles the
 * odd-count case correctly, where a parent's left and right pointers can
 * point to the SAME duplicated node -- naive freeing would double-free
 * that node. Safe to call with NULL. */
void free_merkle_tree(MerkleNode *node);
bool verify_bft_consensus(Block *block, uint32_t total_nodes, uint32_t f_faulty);

/* Real vote-based check: each of total_nodes independently reports the
 * Merkle root it computed (node_roots[i]). Consensus passes only if at
 * least 2f+1 of those reported roots actually MATCH block->merkle_root --
 * this is what verify_bft_consensus was missing: it never looked at any
 * real data, just counted nodes in the abstract. */
bool verify_block_with_votes(Block *block, uint8_t node_roots[][HASH_SIZE],
                              uint32_t total_nodes, uint32_t f_faulty);

#endif
