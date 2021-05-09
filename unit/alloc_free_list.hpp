#include "../src/ckpt_alloc.hpp"
#include "gtest/gtest.h"
#include <limits.h>
#include <stdint.h>

namespace {

    class FreeListAllocTestSuite : public testing::Test {
        protected:
            virtual void SetUp() {
                instance = new GlobalAlloc();
                snapshot = (char *)malloc(FreeList::snapshotSize());
            }

            virtual void TearDown() {
                if (alloc != NULL) delete alloc;
                delete instance;
                free(snapshot);
            }

            FreeList *Factory(uint16_t id = 0) {
                alloc = new FreeList(id);
                return alloc;
            }

            GlobalAlloc *instance = NULL;
            FreeList *alloc = NULL;
            char *snapshot = NULL;
    };

    /*
    TEST_F(FreeListAllocTestSuite, AllocAndFree) {
        // TODO
    }
    */

    TEST_F(FreeListAllocTestSuite, ReallocAndFree) {

        // Prepare environment
        size_t oldSize = 2048;
        size_t newSize = 4096;
        Factory();

        // Re-allocate in-place (shrink next chunk)
        chunk_header_t *p1 = alloc->alloc(oldSize);
        memset((char *)p1 + sizeof(chunk_header_t), 'A',
                oldSize - sizeof(chunk_header_t));
        EXPECT_EQ(p1->size, oldSize);
        EXPECT_EQ(p1->used, 1);
        chunk_header_t *next = (chunk_header_t *)((char *)p1 + oldSize);
        EXPECT_EQ(next->size, FreeList::BlockSize - oldSize);
        EXPECT_EQ(next->used, 0);
        chunk_header_t *p2 = alloc->realloc(p1, newSize);
        EXPECT_EQ(p1, p2);
        memset((char *)p1 + oldSize, 'B', newSize - oldSize);
        EXPECT_EQ(p2->size, newSize);
        next = (chunk_header_t *)((char *)p2 + newSize);
        EXPECT_EQ(next->used, 0);
        EXPECT_EQ(next->size, FreeList::BlockSize - newSize);
        for (size_t i = sizeof(chunk_header_t); i < oldSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'A');
        }
        for (size_t i = oldSize; i < newSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'B');
        }
        chunk_header_t *p2Old = p2;

        // Re-allocate out-of-place
        p1 = alloc->alloc(oldSize);
        memset((char *)p1 + sizeof(chunk_header_t), 'C',
                oldSize - sizeof(chunk_header_t));
        p2 = alloc->realloc(p1, newSize);
        EXPECT_NE(p1, p2);
        memset((char *)p2 + oldSize, 'D', newSize - oldSize);
        EXPECT_EQ(p2->size, newSize);
        EXPECT_EQ(p1->used, 0);
        for (size_t i = sizeof(chunk_header_t); i < oldSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'C');
        }
        for (size_t i = oldSize; i < newSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'D');
        }

        // Re-allocate in-place (use the entire region)
        p1 = p2;
        p2 = alloc->realloc(p1, oldSize + newSize);
        EXPECT_EQ(p1, p2);
        memset((char *)p2 + newSize, 'E', oldSize);
        EXPECT_EQ(p2->size, newSize + oldSize);
        for (size_t i = sizeof(chunk_header_t); i < oldSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'C');
        }
        for (size_t i = oldSize; i < newSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'D');
        }
        for (size_t i = newSize; i < oldSize + newSize; i++) {
            EXPECT_EQ(((char *)p2)[i], 'E');
        }

        // Re-allocate using new block from Global Allocator
        size_t remainingSpace = FreeList::BlockSize - (newSize * 2 + oldSize);
        void *p4 = alloc->alloc(remainingSpace);
        EXPECT_EQ((char *)p4, (char *)p2Old + newSize);
        void *p5 = alloc->realloc(p2Old, newSize + oldSize);
        EXPECT_NE(p2Old, p5);
        memset((char *)p5 + newSize, 'C', oldSize);
        for (int i = sizeof(chunk_header_t); i < oldSize + newSize; i++) {
            char expectedValue = 'C';
            if (i < newSize) expectedValue = 'B';
            if (i < oldSize) expectedValue = 'A';
            EXPECT_EQ(((char *)p5)[i], expectedValue);
        }
    }

    TEST_F(FreeListAllocTestSuite, SaveSnapshot) {
        FreeList *allocator = Factory();

        // Save empty free-list
        memset(snapshot, 1, FreeList::snapshotSize());
        alloc->save(snapshot);
        for (size_t i = 0; i < FreeList::snapshotSize(); i++) {
            EXPECT_EQ(snapshot[i], 0);
        }

        // Populate free-list and save again
        uintptr_t ptrs[TOTAL_ALLOC_BUCKETS];
        uint64_t total_allocated = 0;
        for (int i = 1; i <= TOTAL_ALLOC_BUCKETS; i++) {
            ptrs[i - 1] = (uintptr_t)alloc->alloc(32 << i);
            total_allocated += (32 << i);
        }
        alloc->save(snapshot);
        uint64_t *ckpt = (uint64_t *)snapshot;
        EXPECT_EQ(ckpt[0], 0);
        EXPECT_EQ(ckpt[1], total_allocated);
        EXPECT_EQ(ckpt[2], 1); // global allocations
        for (int i = 1; i < TOTAL_ALLOC_BUCKETS; i++) {
            EXPECT_EQ(ckpt[3 + i], 0);
        }
        EXPECT_NE(ckpt[3 + TOTAL_ALLOC_BUCKETS], 0);

        // Free allocated regions and save again
        for (int i = 0; i < TOTAL_ALLOC_BUCKETS; i++) {
            alloc->dealloc((chunk_header_t *)ptrs[i]);
        }
        alloc->save(snapshot);
        EXPECT_EQ(ckpt[0], 0);
        EXPECT_EQ(ckpt[1], 0);
        EXPECT_EQ(ckpt[2], 1);
        for (int i = 0; i < TOTAL_ALLOC_BUCKETS - 1; i++) {
            EXPECT_EQ(ckpt[4 + i], 0);
        }
        EXPECT_EQ(ckpt[3 + TOTAL_ALLOC_BUCKETS], ptrs[0]);
    }

    TEST_F(FreeListAllocTestSuite, LoadSnapshot) {
        size_t snapshotSize = FreeList::snapshotSize();
        EXPECT_EQ(snapshotSize, sizeof(uint64_t) * (TOTAL_ALLOC_BUCKETS + 4));

        // Setup memory block
        uint64_t freeListId = 1;
        uintptr_t ptrs[TOTAL_ALLOC_BUCKETS];
        uintptr_t baseAddress = GlobalAlloc::BaseAddress;
        uint64_t totalAllocated = 0;
        for (int i = 0; i < TOTAL_ALLOC_BUCKETS; i++) {
            size_t regionSize = 32 << (i + 1);
            free_header_t *fc = (free_header_t *)baseAddress;
            fc->prev_offset = (baseAddress - GlobalAlloc::BaseAddress) / 64;
            fc->free_list_id = freeListId;
            fc->last = 0;
            fc->jumbo = 0;
            fc->size = regionSize;
            fc->prev = fc->next = NULL;
            ptrs[i] = baseAddress;
            baseAddress += regionSize;

            chunk_header_t *chunk = (chunk_header_t *)baseAddress;
            chunk->prev_offset = (baseAddress - GlobalAlloc::BaseAddress) / 64;
            chunk->free_list_id = freeListId;
            chunk->last = 0;
            chunk->jumbo = 0;
            chunk->size = regionSize;
            totalAllocated += regionSize;
            baseAddress += regionSize;
        }
        chunk_header_t *chunk = (chunk_header_t *)baseAddress;
        chunk->prev_offset = (baseAddress - GlobalAlloc::BaseAddress) / 64;
        chunk->free_list_id = freeListId;
        chunk->last = 1;
        chunk->jumbo = 0;
        chunk->size = FreeList::BlockSize - totalAllocated * 2;
        totalAllocated += chunk->size;

        // Setup snapshot
        uint64_t *ckpt = (uint64_t *)snapshot;
        ckpt[0] = freeListId;
        ckpt[1] = totalAllocated;
        ckpt[2] = 123;
        ckpt[3] = 456;
        for (int i = 0; i < TOTAL_ALLOC_BUCKETS; i++) {
            ckpt[4 + i] = ptrs[i];
        }

        // Load snapshot
        alloc = Factory(freeListId);
        alloc->load(snapshot);

        // Check allocation table
        for (int i = 0; i < TOTAL_ALLOC_BUCKETS; i++) {
            size_t regionSize = 32 << (i + 1);
            uintptr_t ptr = (uintptr_t)alloc->alloc(regionSize);
            EXPECT_EQ(ptrs[i], ptr);
        }

        // Check snapshot
        size_t blockSize = FreeList::BlockSize;
        memset(snapshot, 0, alloc->snapshotSize());
        alloc->save(snapshot);
        EXPECT_EQ(ckpt[0], freeListId);
        EXPECT_EQ(ckpt[1], blockSize);
        EXPECT_EQ(ckpt[2], 123);
        EXPECT_EQ(ckpt[3], 456 + TOTAL_ALLOC_BUCKETS);
        for (int i = 0; i < TOTAL_ALLOC_BUCKETS; i++) {
            EXPECT_EQ(ckpt[4 + i], 0);
        }
    }
}
