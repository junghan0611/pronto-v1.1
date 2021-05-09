#include "../src/ckpt_alloc.hpp"
#include "gtest/gtest.h"
#include <limits.h>
#include <stdint.h>
#include <emmintrin.h>

namespace {

    class GlobalAllocTestSuite : public testing::Test {
        protected:
            virtual void SetUp() {
                instance = GlobalAlloc::getInstance();
                snapshotSize = instance->snapshotSize();
                snapshot = (char *)malloc(snapshotSize);
            }

            virtual void TearDown() {
                delete instance;
                free(snapshot);
            }

            GlobalAlloc *instance = NULL;
            size_t snapshotSize;
            char *snapshot = NULL;
    };

    TEST_F(GlobalAllocTestSuite, AllocAndFree) {
        size_t blockSize = FreeList::BlockSize;
        size_t poolSize = GlobalAlloc::MinPoolSize;
        uintptr_t baseAddress = GlobalAlloc::BaseAddress;
        size_t freeBlocks = poolSize / blockSize;

        uintptr_t expected_addr = baseAddress + poolSize - blockSize;
        void **ptrs = (void **)malloc(sizeof(void *) * freeBlocks);
        for (size_t i = 0; i < freeBlocks; i++) {
            ptrs[i] = instance->alloc(blockSize);
            EXPECT_EQ((uintptr_t)ptrs[i], expected_addr);
            expected_addr -= blockSize;
        }

        for (size_t i = 0; i < freeBlocks; i++) {
            instance->release(ptrs[i], blockSize);
            uintptr_t ptr = (uintptr_t)instance->alloc(blockSize);
            EXPECT_EQ(ptr, (uintptr_t)ptrs[i]);
            instance->alloc(blockSize);
        }
    }

    TEST_F(GlobalAllocTestSuite, SaveSnapshot) {
        uintptr_t *ptr = (uintptr_t *)snapshot;
        size_t minPoolSize = GlobalAlloc::MinPoolSize;
        uintptr_t baseAddress = GlobalAlloc::BaseAddress;
        size_t blockSize = FreeList::BlockSize;
        void *ptrs[3];

        // No allocations
        _mm_clflush((void *)baseAddress);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 1);
        EXPECT_EQ(ptr[2], baseAddress);
        EXPECT_EQ(ptr[3], minPoolSize);

        // Allocate a few and check the snapshot
        _mm_clflush((void *)baseAddress);
        _mm_clflush((void *)(baseAddress + 64));
        _mm_clflush((void *)(baseAddress + 128));
        _mm_clflush((void *)(baseAddress + 192));
        ptrs[0] = instance->alloc(blockSize);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 1);
        EXPECT_EQ(ptr[2], baseAddress);
        EXPECT_EQ(ptr[3], minPoolSize - blockSize);

        ptrs[1] = instance->alloc(blockSize);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 1);
        EXPECT_EQ(ptr[2], baseAddress);
        EXPECT_EQ(ptr[3], minPoolSize - blockSize * 2);

        ptrs[2] = instance->alloc(blockSize);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 1);
        EXPECT_EQ(ptr[2], baseAddress);
        EXPECT_EQ(ptr[3], minPoolSize - blockSize * 3);

        // Free the allocated regions
        instance->release(ptrs[0], blockSize);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 2);
        EXPECT_EQ(ptr[2], (uintptr_t)ptrs[0]);
        EXPECT_EQ(ptr[3], blockSize);
        EXPECT_EQ(ptr[4], baseAddress);
        EXPECT_EQ(ptr[5], minPoolSize - blockSize * 3);

        instance->release(ptrs[1], blockSize);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 2);
        EXPECT_EQ(ptr[2], (uintptr_t)ptrs[1]);
        EXPECT_EQ(ptr[3], blockSize * 2);
        EXPECT_EQ(ptr[4], baseAddress);
        EXPECT_EQ(ptr[5], minPoolSize - blockSize * 3);

        instance->release(ptrs[2], blockSize);
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize);
        EXPECT_EQ(ptr[1], 1);
        EXPECT_EQ(ptr[2], baseAddress);
        EXPECT_EQ(ptr[3], minPoolSize);

        // Make Global Allocator mmap more memory
        size_t blocksPerMap = GlobalAlloc::MinPoolSize / blockSize;
        void **blocks = (void **)malloc(sizeof(void *) * (blocksPerMap + 1));
        for (size_t i = 0; i <= blocksPerMap; i++) {
            blocks[i] = instance->alloc(blockSize);
        }
        instance->save(snapshot);
        EXPECT_EQ(ptr[0], minPoolSize * 2);
        EXPECT_EQ(ptr[1], 1);
        EXPECT_EQ(ptr[2], baseAddress + minPoolSize);
        EXPECT_EQ(ptr[3], minPoolSize - blockSize);
    }

    TEST_F(GlobalAllocTestSuite, LoadSnapshot) {
        // Initialize environment
        delete instance;
        uintptr_t *ptrs = (uintptr_t *)snapshot;
        ptrs[0] = GlobalAlloc::MinPoolSize * 2;
        ptrs[1] = 4;
        // Free region #1
        ptrs[2] = GlobalAlloc::BaseAddress;
        ptrs[3] = FreeList::BlockSize;
        // Free region #2
        ptrs[4] = GlobalAlloc::BaseAddress +
            FreeList::BlockSize * 2;
        ptrs[5] = FreeList::BlockSize;
        // Free region #3
        ptrs[6] = GlobalAlloc::BaseAddress +
            FreeList::BlockSize * 4;
        ptrs[7] = FreeList::BlockSize;
        // Free region #4
        ptrs[8] = GlobalAlloc::BaseAddress +
            FreeList::BlockSize * 8;
        ptrs[9] = FreeList::BlockSize;
        // Free region #5 (must be ignored)
        ptrs[10] = GlobalAlloc::BaseAddress +
            FreeList::BlockSize * 16;
        ptrs[11] = FreeList::BlockSize;

        // Load from snapshot
        instance = new GlobalAlloc(snapshot);

        // Verify correctness by allocation
        for (size_t i = 0; i < ptrs[1]; i++) {
            void *ptr = instance->alloc(FreeList::BlockSize);
            EXPECT_EQ((uintptr_t)ptr, (uintptr_t)ptrs[2 + 2 * i]);
        }

        uintptr_t expected = GlobalAlloc::BaseAddress +
            GlobalAlloc::MinPoolSize + ptrs[0] - FreeList::BlockSize;
        void *ptr = instance->alloc(FreeList::BlockSize);
        EXPECT_EQ((uintptr_t)ptr, expected);
    }
}
