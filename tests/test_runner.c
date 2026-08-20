#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "chronos.h"

int main(void) {
    const char *db_file = "test_run.db";
    uint64_t max_records = 1000;
    remove(db_file);

    ChronosContext ctx;
    assert(chronos_init(&ctx, db_file, max_records) == 0);

    chronos_log_aegis_event(&ctx, 1, "AEGIS_TOKEN_PRUNED: ID=101");
    chronos_log_vortx_event(&ctx, 2, "VORTX_VM_LAUNCH: VM_ID=402");
    chronos_log_vortx_event(&ctx, 3, "VORTX_VM_HALT: VM_ID=402");
    assert(ctx.header->head_index == 3);

    assert(chronos_verify_integrity(&ctx) == true);

    ChronosRecord results[10];
    size_t fetched = chronos_query_time_range(&ctx, 0, UINT64_MAX, results, 10);
    assert(fetched == 3);

    chronos_close(&ctx);
    remove(db_file);
    printf("test_runner: ALL TESTS PASSED\n");
    return 0;
}
