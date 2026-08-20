#ifndef CHRONOS_H
#define CHRONOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pthread.h>

#define CHRONOS_MAGIC 0x4348524F
#define CHRONOS_VERSION 1

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t record_size;
    uint64_t total_capacity;
    uint64_t head_index;
    uint64_t tail_index;
    uint8_t last_hash[32];
} ChronosHeader;

typedef struct {
    uint64_t timestamp_ns;
    uint32_t module_id;
    uint32_t severity;
    uint8_t payload[80];
    uint8_t prev_hash[32];
} ChronosRecord;

typedef struct {
    int fd;
    size_t file_size;
    char file_path[256];
    ChronosHeader *header;
    ChronosRecord *records;
    pthread_t sync_thread;
    bool sync_thread_running;
    uint32_t sync_interval_ms;
    pthread_mutex_t hash_lock;   // guards head_index + hash chaining together
} ChronosContext;

int chronos_init(ChronosContext *ctx, const char *file_path, uint64_t max_records);
int chronos_append(ChronosContext *ctx, uint32_t module_id, uint32_t severity, const uint8_t *payload, size_t payload_len);
void chronos_flush(ChronosContext *ctx, bool sync_mode);
bool chronos_verify_integrity(ChronosContext *ctx);
size_t chronos_query_time_range(ChronosContext *ctx, uint64_t start_ns, uint64_t end_ns, ChronosRecord *out_buffer, size_t max_out);
int chronos_start_bg_sync(ChronosContext *ctx, uint32_t interval_ms);
void chronos_stop_bg_sync(ChronosContext *ctx);
void chronos_close(ChronosContext *ctx);
int chronos_log_aegis_event(ChronosContext *ctx, uint32_t severity, const char *token_msg);
int chronos_log_vortx_event(ChronosContext *ctx, uint32_t severity, const char *vm_msg);

#endif