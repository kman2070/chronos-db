#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 600
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#define OPENSSL_SUPPRESS_DEPRECATED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <pthread.h>
#include <openssl/sha.h>
#include "chronos.h"

int chronos_init(ChronosContext *ctx, const char *file_path, uint64_t max_records) {
    if (!ctx || !file_path) return -1;
    memset(ctx, 0, sizeof(ChronosContext));
    strncpy(ctx->file_path, file_path, sizeof(ctx->file_path) - 1);

    size_t required_size = sizeof(ChronosHeader) + (max_records * sizeof(ChronosRecord));
    ctx->file_size = required_size;

    ctx->fd = open(file_path, O_RDWR | O_CREAT, 0644);
    if (ctx->fd < 0) return -1;

    int alloc_res = posix_fallocate(ctx->fd, 0, (off_t)required_size);
    if (alloc_res != 0) {
        close(ctx->fd);
        return -1;
    }

    void *map = mmap(NULL, required_size, PROT_READ | PROT_WRITE, MAP_SHARED, ctx->fd, 0);
    if (map == MAP_FAILED) {
        close(ctx->fd);
        return -1;
    }

    ctx->header = (ChronosHeader *)map;
    ctx->records = (ChronosRecord *)((uint8_t *)map + sizeof(ChronosHeader));

    if (ctx->header->magic != CHRONOS_MAGIC) {
        ctx->header->magic = CHRONOS_MAGIC;
        ctx->header->version = CHRONOS_VERSION;
        ctx->header->record_size = sizeof(ChronosRecord);
        ctx->header->total_capacity = max_records;
        ctx->header->head_index = 0;
        ctx->header->tail_index = 0;
        memset(ctx->header->last_hash, 0, SHA256_DIGEST_LENGTH);
    }

    if (pthread_mutex_init(&ctx->hash_lock, NULL) != 0) {
        munmap(map, required_size);
        close(ctx->fd);
        return -1;
    }

    return 0;
}

int chronos_append(ChronosContext *ctx, uint32_t module_id, uint32_t severity, const uint8_t *payload, size_t payload_len) {
    if (!ctx || !ctx->header) return -1;

    pthread_mutex_lock(&ctx->hash_lock);

    uint64_t idx = ctx->header->head_index++;
    uint64_t slot = idx % ctx->header->total_capacity;

    ChronosRecord *rec = &ctx->records[slot];
    memset(rec, 0, sizeof(ChronosRecord));

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    rec->timestamp_ns = (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;

    rec->module_id = module_id;
    rec->severity = severity;

    if (payload && payload_len > 0) {
        size_t copy_len = payload_len > 80 ? 80 : payload_len;
        memcpy(rec->payload, payload, copy_len);
    }

    memcpy(rec->prev_hash, ctx->header->last_hash, SHA256_DIGEST_LENGTH);

    SHA256_CTX sha_ctx;
    SHA256_Init(&sha_ctx);
    SHA256_Update(&sha_ctx, rec, offsetof(ChronosRecord, prev_hash));
    SHA256_Update(&sha_ctx, rec->prev_hash, SHA256_DIGEST_LENGTH);
    SHA256_Final(ctx->header->last_hash, &sha_ctx);

    pthread_mutex_unlock(&ctx->hash_lock);

    return 0;
}

void chronos_flush(ChronosContext *ctx, bool sync_mode) {
    if (!ctx || !ctx->header) return;
    msync(ctx->header, ctx->file_size, sync_mode ? MS_SYNC : MS_ASYNC);
}

bool chronos_verify_integrity(ChronosContext *ctx) {
    if (!ctx || !ctx->header || !ctx->records) return false;

    uint64_t head = ctx->header->head_index;
    uint64_t cap = ctx->header->total_capacity;

    if (head == 0) return true;

    uint64_t count = (head > cap) ? cap : head;
    uint64_t start_seq = head - count;

    for (uint64_t i = 0; i < count; i++) {
        uint64_t current_seq = start_seq + i;
        uint64_t slot = current_seq % cap;
        ChronosRecord *rec = &ctx->records[slot];

        unsigned char computed_hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha_ctx;
        SHA256_Init(&sha_ctx);
        SHA256_Update(&sha_ctx, rec, offsetof(ChronosRecord, prev_hash));
        SHA256_Update(&sha_ctx, rec->prev_hash, SHA256_DIGEST_LENGTH);
        SHA256_Final(computed_hash, &sha_ctx);

        if (i < count - 1) {
            uint64_t next_slot = (current_seq + 1) % cap;
            ChronosRecord *next_rec = &ctx->records[next_slot];
            if (memcmp(computed_hash, next_rec->prev_hash, SHA256_DIGEST_LENGTH) != 0) {
                return false;
            }
        } else {
            if (memcmp(computed_hash, ctx->header->last_hash, SHA256_DIGEST_LENGTH) != 0) {
                return false;
            }
        }
    }

    return true;
}

size_t chronos_query_time_range(ChronosContext *ctx, uint64_t start_ns, uint64_t end_ns, ChronosRecord *out_buffer, size_t max_out) {
    if (!ctx || !ctx->header || !ctx->records || !out_buffer || max_out == 0) return 0;

    uint64_t head = ctx->header->head_index;
    uint64_t cap = ctx->header->total_capacity;
    if (head == 0) return 0;

    uint64_t count = (head > cap) ? cap : head;
    uint64_t start_seq = head - count;

    int64_t low = 0;
    int64_t high = (int64_t)count - 1;
    int64_t first_idx = -1;

    while (low <= high) {
        int64_t mid = low + (high - low) / 2;
        uint64_t slot = (start_seq + mid) % cap;

        if (ctx->records[slot].timestamp_ns >= start_ns) {
            first_idx = mid;
            high = mid - 1;
        } else {
            low = mid + 1;
        }
    }

    if (first_idx == -1) return 0;

    size_t copied = 0;
    for (uint64_t i = (uint64_t)first_idx; i < count && copied < max_out; i++) {
        uint64_t slot = (start_seq + i) % cap;

        if (ctx->records[slot].timestamp_ns > end_ns) {
            break;
        }

        out_buffer[copied++] = ctx->records[slot];
    }

    return copied;
}

static void* bg_sync_worker(void *arg) {
    ChronosContext *ctx = (ChronosContext *)arg;
    while (ctx->sync_thread_running) {
        usleep(ctx->sync_interval_ms * 1000);
        chronos_flush(ctx, false);
    }
    pthread_exit(NULL);
}

int chronos_start_bg_sync(ChronosContext *ctx, uint32_t interval_ms) {
    if (!ctx || ctx->sync_thread_running) return -1;

    ctx->sync_interval_ms = interval_ms;
    ctx->sync_thread_running = true;

    if (pthread_create(&ctx->sync_thread, NULL, bg_sync_worker, ctx) != 0) {
        ctx->sync_thread_running = false;
        return -1;
    }
    return 0;
}

void chronos_stop_bg_sync(ChronosContext *ctx) {
    if (!ctx || !ctx->sync_thread_running) return;

    ctx->sync_thread_running = false;
    pthread_join(ctx->sync_thread, NULL);
}

void chronos_close(ChronosContext *ctx) {
    if (!ctx) return;
    if (ctx->sync_thread_running) {
        chronos_stop_bg_sync(ctx);
    }
    pthread_mutex_destroy(&ctx->hash_lock);
    if (ctx->header) {
        munmap(ctx->header, ctx->file_size);
    }
    if (ctx->fd >= 0) {
        close(ctx->fd);
    }
}

int chronos_log_aegis_event(ChronosContext *ctx, uint32_t severity, const char *token_msg) {
    uint32_t aegis_module_id = 0x01;
    size_t len = 0;
    while (token_msg && token_msg[len] != '\0' && len < 80) len++;
    return chronos_append(ctx, aegis_module_id, severity, (const uint8_t *)token_msg, len);
}

int chronos_log_vortx_event(ChronosContext *ctx, uint32_t severity, const char *vm_msg) {
    uint32_t vortx_module_id = 0x02;
    size_t len = 0;
    while (vm_msg && vm_msg[len] != '\0' && len < 80) len++;
    return chronos_append(ctx, vortx_module_id, severity, (const uint8_t *)vm_msg, len);
}
#include <stdbool.h>

static bool is_active = false;

bool try_become_active(void) {
    if (!is_active) {
        is_active = true;
        return true;
    }
    return false;
}

void release_active(void) {
    is_active = false;
}