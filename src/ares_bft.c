#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ares_bft.h"
#include <openssl/sha.h>

void compute_sha256(const unsigned char *data, size_t len, uint8_t *output) {
    SHA256(data, len, output);
}

MerkleNode* build_merkle_tree(LogEntry *logs, size_t count) {
    if (count == 0) return NULL;

    MerkleNode **nodes = malloc(count * sizeof(MerkleNode*));
    for (size_t i = 0; i < count; i++) {
        nodes[i] = malloc(sizeof(MerkleNode));
        compute_sha256((unsigned char*)logs[i].log_data, strlen(logs[i].log_data), nodes[i]->hash);
        nodes[i]->left = NULL;
        nodes[i]->right = NULL;
    }

    size_t current_count = count;
    while (current_count > 1) {
        size_t new_count = (current_count + 1) / 2;
        MerkleNode **parent_nodes = malloc(new_count * sizeof(MerkleNode*));

        for (size_t i = 0; i < new_count; i++) {
            parent_nodes[i] = malloc(sizeof(MerkleNode));
            parent_nodes[i]->left = nodes[i * 2];

            if (i * 2 + 1 < current_count) {
                parent_nodes[i]->right = nodes[i * 2 + 1];
            } else {
                parent_nodes[i]->right = nodes[i * 2];
            }

            uint8_t combined[HASH_SIZE * 2];
            memcpy(combined, parent_nodes[i]->left->hash, HASH_SIZE);
            memcpy(combined + HASH_SIZE, parent_nodes[i]->right->hash, HASH_SIZE);
            compute_sha256(combined, HASH_SIZE * 2, parent_nodes[i]->hash);
        }

        free(nodes);
        nodes = parent_nodes;
        current_count = new_count;
    }

    MerkleNode *root = nodes[0];
    free(nodes);
    return root;
}

bool verify_bft_consensus(Block *block, uint32_t total_nodes, uint32_t f_faulty) {
    uint32_t required_votes = 2 * f_faulty + 1;
    uint32_t simulated_valid_votes = total_nodes - f_faulty;

    printf("\n--- Ares-BFT Consensus Check ---\n");
    printf("Block ID: %u\n", block->block_id);
    printf("Total Nodes: %u | Faulty Allowed (f): %u\n", total_nodes, f_faulty);
    printf("Required Quorum: %u votes\n", required_votes);

    if (simulated_valid_votes >= required_votes) {
        printf("Result: CONSENSUS PASSED!\n");
        return true;
    } else {
        printf("Result: CONSENSUS FAILED!\n");
        return false;
    }
}

bool verify_block_with_votes(Block *block, uint8_t node_roots[][HASH_SIZE],
                              uint32_t total_nodes, uint32_t f_faulty) {
    uint32_t required_votes = 2 * f_faulty + 1;
    uint32_t matching_votes = 0;

    for (uint32_t i = 0; i < total_nodes; i++) {
        if (memcmp(node_roots[i], block->merkle_root, HASH_SIZE) == 0) {
            matching_votes++;
        }
    }

    printf("\n--- Ares-BFT Vote Verification (Block %u) ---\n", block->block_id);
    printf("Nodes agreeing with claimed Merkle root: %u / %u\n", matching_votes, total_nodes);
    printf("Required Quorum: %u votes\n", required_votes);

    if (matching_votes >= required_votes) {
        printf("Result: CONSENSUS PASSED (block accepted)\n");
        return true;
    } else {
        printf("Result: CONSENSUS FAILED (not enough nodes agree -- possible tampering)\n");
        return false;
    }
}

void free_merkle_tree(MerkleNode *node) {
    if (!node) return;

    if (node->left == node->right) {
        /* Either a leaf (both NULL) or the odd-count duplicate case
         * (both point to the same child) -- recurse once either way,
         * never twice into the same pointer. */
        free_merkle_tree(node->left);
    } else {
        free_merkle_tree(node->left);
        free_merkle_tree(node->right);
    }

    free(node);
}
