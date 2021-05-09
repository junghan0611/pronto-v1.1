#include <assert.h>
#include <uuid/uuid.h>
#include <time.h>
#include <stdio.h>
#include "../../src/ckpt_alloc.hpp"

#define TEST_UUID   "3b24574c-e920-4066-8eec-92f9e4682702"
#define OPS_COUNT   1E6
#ifndef ALLOC_SIZE
#define ALLOC_SIZE  1024
#endif
#define TIMING

int main() {
    uuid_t uuid;
    assert(uuid_parse(TEST_UUID, uuid) == 0);

    GlobalAlloc::getInstance();
    ObjectAlloc alloc(uuid);
    void **ptrs = (void **)malloc(sizeof(void *) * OPS_COUNT);

#ifdef TIMING
    struct timespec t1, t2;
    clock_gettime(CLOCK_REALTIME, &t1);
#endif

    for (size_t op = 0; op < OPS_COUNT; op++) {
        ptrs[op] = alloc.alloc(ALLOC_SIZE);
    }

#ifdef TIMING
    clock_gettime(CLOCK_REALTIME, &t2);
    uint64_t total_time = (t2.tv_sec - t1.tv_sec) * 1E9 + t2.tv_nsec - t1.tv_nsec;
    fprintf(stdout, "Alloc average = %0.2f ns\n", (double)total_time / OPS_COUNT);
#endif

#ifdef TIMING
    clock_gettime(CLOCK_REALTIME, &t1);
#endif

    for (size_t op = 0; op < OPS_COUNT; op++) {
        alloc.dealloc(ptrs[op]);
    }

#ifdef TIMING
    clock_gettime(CLOCK_REALTIME, &t2);
    total_time = (t2.tv_sec - t1.tv_sec) * 1E9 + t2.tv_nsec - t1.tv_nsec;
    fprintf(stdout, "Free average = %0.2f ns\n", (double)total_time / OPS_COUNT);

#endif

    free(ptrs);
    return 0;
}
