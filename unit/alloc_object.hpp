#include "../src/ckpt_alloc.hpp"
#include "gtest/gtest.h"
#include <limits.h>
#include <stdint.h>

void get_cpu_info(uint8_t *, int *);

namespace {

    class ObjectAllocTestSuite : public testing::Test {
        protected:
            virtual void SetUp() {
                instance = new GlobalAlloc();
            }

            virtual void TearDown() {
                if (alloc != NULL) delete alloc;
                if (snapshot != NULL) free(snapshot);
                delete instance;
            }

            ObjectAlloc *Factory(const char *ckpt = NULL) {
                if (ckpt == NULL) uuid_generate(uuid);
                alloc = new ObjectAlloc(uuid, ckpt);
                if (snapshot == NULL)
                    snapshot = (char *)malloc(alloc->snapshotSize());
                return alloc;
            }

            uuid_t uuid;
            GlobalAlloc *instance = NULL;
            ObjectAlloc *alloc = NULL;
            char *snapshot = NULL;
    };

    TEST_F(ObjectAllocTestSuite, AllocAndFree) {
        // TODO
    }

    TEST_F(ObjectAllocTestSuite, ReallocAndFree) {
        // TODO
    }

    TEST_F(ObjectAllocTestSuite, AllocBigRegion) {
        const size_t BlockSize = FreeList::BlockSize;
        const size_t PageSize = GlobalAlloc::BitmapGranularity;
        const uintptr_t BaseAddr = GlobalAlloc::BaseAddress;
        const size_t MinPoolSize = GlobalAlloc::MinPoolSize;

        Factory();
        void *ptrs[2] = {
            alloc->alloc(BlockSize),
            alloc->alloc(PageSize),
        };

        uintptr_t expected[2] = {
            BaseAddr + MinPoolSize - 2 * BlockSize,
            BaseAddr + MinPoolSize - 3 * BlockSize,
        };

        EXPECT_EQ((uintptr_t)ptrs[0], expected[0] + sizeof(chunk_header_t));
        EXPECT_EQ((uintptr_t)ptrs[1], expected[1] + sizeof(chunk_header_t));
    }

    TEST_F(ObjectAllocTestSuite, DeallocBigRegion) {
        const size_t BlockSize = FreeList::BlockSize;
        const size_t AllocSize = BlockSize * 3 - sizeof(chunk_header_t);

        Factory();
        void *ptr0 = alloc->alloc(AllocSize);
        EXPECT_NE((uintptr_t)ptr0, 0);
        alloc->dealloc(ptr0);

        void *ptr1 = alloc->alloc(AllocSize);
        EXPECT_EQ(ptr0, ptr1);
    }

    TEST_F(ObjectAllocTestSuite, SaveSnapshot) {
        uint8_t core_info[64];
        int cores;
        get_cpu_info(core_info, &cores);
        alloc = Factory();

        // Verify snapshot size
        size_t snapshotSize = alloc->snapshotSize();
        size_t expectedSize = sizeof(uuid_t) + sizeof(uint64_t) +
            sizeof(uintptr_t);
        expectedSize += cores * FreeList::snapshotSize();
        EXPECT_EQ(expectedSize, snapshotSize);

        // Verify blank checkpoint
        alloc->save(snapshot);
        uuid_t snapshotUUID;
        uint64_t *ptr = (uint64_t *)snapshot;
        uint64_t *uuidPtr = (uint64_t *)snapshotUUID;
        uuidPtr[0] = ptr[0];
        uuidPtr[1] = ptr[1];
        EXPECT_EQ(uuid_compare(uuid, snapshotUUID), 0);
        EXPECT_EQ(ptr[2], cores);
        EXPECT_EQ(ptr[3], (uint64_t)alloc);
        ptr = &ptr[4];
        for (size_t i = 0; i < cores; i++) {
            EXPECT_EQ(ptr[0], i);
            EXPECT_EQ(ptr[1], 0);
            EXPECT_EQ(ptr[2], 0);
            EXPECT_EQ(ptr[3], 0);
            ptr = &ptr[4];

            for (int j = 0; j < TOTAL_ALLOC_BUCKETS; j++) {
                EXPECT_EQ(ptr[j], 0);
            }
            ptr += TOTAL_ALLOC_BUCKETS;
        }

        // Allocate and free a few regions to populate free-lists
        void *pointers[4] = {
            alloc->alloc(2048 - sizeof(chunk_header_t)),
            alloc->alloc(1024 - sizeof(chunk_header_t)),
            alloc->alloc(4096 - sizeof(chunk_header_t)),
            alloc->alloc(512 - sizeof(chunk_header_t)),
        };
        alloc->dealloc(pointers[2]);
        chunk_header_t *chunk = (chunk_header_t *)((char *)pointers[0] -
                sizeof(chunk_header_t));

        // Verify non-empty snapshot
        alloc->save(snapshot);
        ptr = (uint64_t *)snapshot;
        uuidPtr[0] = ptr[0];
        uuidPtr[1] = ptr[1];
        EXPECT_EQ(uuid_compare(uuid, snapshotUUID), 0);
        EXPECT_EQ(ptr[2], cores);
        EXPECT_EQ(ptr[3], (uint64_t)alloc);
        ptr = &ptr[4];
        for (size_t i = 0; i < cores; i++) {
            EXPECT_EQ(ptr[0], i);
            if (chunk->free_list_id == i) {
                EXPECT_EQ(ptr[1], 2048 + 1024 + 512);
                EXPECT_EQ(ptr[2], 1);
                EXPECT_NE(ptr[3], 0);
                size_t emptyBuckets = 0;
                size_t nonEmptyBuckets = 0;
                for (int j = 0; j < TOTAL_ALLOC_BUCKETS; j++) {
                    if (ptr[4 + j] != 0) nonEmptyBuckets++;
                    else emptyBuckets++;
                }
                EXPECT_EQ(emptyBuckets, TOTAL_ALLOC_BUCKETS - 2);
                EXPECT_EQ(nonEmptyBuckets, 2);
            }
            else {
                EXPECT_EQ(ptr[1], 0);
                EXPECT_EQ(ptr[2], 0);
                EXPECT_EQ(ptr[3], 0);
                for (int j = 0; j < TOTAL_ALLOC_BUCKETS; j++) {
                    EXPECT_EQ(ptr[4 + j], 0);
                }
            }
            ptr += TOTAL_ALLOC_BUCKETS + 4;
        }
    }

    TEST_F(ObjectAllocTestSuite, LoadSnapshot) {

        // Prepare environment
        uint8_t core_info[64];
        int cores;
        get_cpu_info(core_info, &cores);
        size_t snapshotSize =
            sizeof(uint64_t) * 4 + cores * FreeList::snapshotSize();
        snapshot = (char *)malloc(snapshotSize);
        uint64_t *ckpt = (uint64_t *)snapshot;

        // Create snapshot
        uuid_generate(uuid);
        ckpt[0] = ((uint64_t *)uuid)[0];
        ckpt[1] = ((uint64_t *)uuid)[1];
        ckpt[2] = cores;
        ckpt[3] = 0;
        ckpt = &ckpt[4];
        for (int i = 0; i < cores; i++) {
            ckpt[0] = i;
            ckpt[1] = 0;
            ckpt[2] = 1;
            ckpt[3] = 0;
            for (int j = 0; j < TOTAL_ALLOC_BUCKETS - 1; j++) {
                ckpt[3 + j] = 0;
            }
            ckpt[3 + TOTAL_ALLOC_BUCKETS - 1] =
                GlobalAlloc::BaseAddress + i * FreeList::BlockSize;
            free_header_t *fc =
                (free_header_t *)ckpt[3 + TOTAL_ALLOC_BUCKETS - 1];
            fc->prev_offset = 0;
            fc->size = FreeList::BlockSize;
            fc->used = 0;
            fc->jumbo = 0;
            fc->next = fc->prev = NULL;
            fc->free_list_id = i;
            ckpt += TOTAL_ALLOC_BUCKETS + 4;
        }

        // Load from snapshot
        alloc = Factory(snapshot);

        // Verify load by allocating memory
        void *ptr = alloc->alloc(4096);
        chunk_header_t *chunk =
            (chunk_header_t *)((char *)ptr - sizeof(chunk_header_t));
        uintptr_t expectedAddress = GlobalAlloc::BaseAddress +
            (chunk->free_list_id + 1) * FreeList::BlockSize -
            chunk->size;
        EXPECT_EQ((uintptr_t)chunk, expectedAddress);
    }
}
