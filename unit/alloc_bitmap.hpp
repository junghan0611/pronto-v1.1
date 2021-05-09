#include "../src/ckpt_alloc.hpp"
#include "gtest/gtest.h"
#include <limits.h>
#include <stdint.h>

namespace {

    class BitmapTestSuite : public testing::Test {
        protected:
            virtual void SetUp() {
                instance = GlobalAlloc::getInstance();
                bitmap = (uint64_t *)malloc(instance->bitmapSize());
            }

            virtual void TearDown() {
                if (alloc != NULL) {
                    delete alloc;
                }
                delete instance;
                free(bitmap);
            }

            uintptr_t GetPointer(off_t offset) {
                return GlobalAlloc::BaseAddress + offset;
            }

            ObjectAlloc *SetUpAlloc() {
                uuid_t uuid;
                uuid_generate(uuid);
                alloc = new ObjectAlloc(uuid);
                return alloc;
            }

            void SyncBitmaps() {
                instance->saveBitmap((char *)bitmap);
            }

            void CheckBitmap(uintptr_t ptr, uint64_t value) {
                uint64_t offset = (ptr - GlobalAlloc::BaseAddress) >> 18;
                EXPECT_EQ(bitmap[offset], value);
            }

            GlobalAlloc *instance = NULL;
            uint64_t *bitmap = NULL;

            ObjectAlloc *alloc = NULL;
    };

    // Test set bitmap
    TEST_F(BitmapTestSuite, SetSmallRegionAtPageHead) {
        size_t region_size = 1024;

        // First page in block
        instance->setBitmap(GetPointer(256 << 10), region_size);

        // Mid block
        instance->setBitmap(GetPointer((256 + 4) << 10), region_size);

        // End of block
        instance->setBitmap(GetPointer((512 - 4) << 10), region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x8000000000000003, bitmap[1]);
        EXPECT_EQ(0x0000000000000000, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetSmallRegionAtPageTail) {
        size_t region_size = 2048;

        // First page in block
        instance->setBitmap(GetPointer(((256 + 4) << 10) - region_size),
                region_size);

        // Mid block
        instance->setBitmap(GetPointer(((256 + 40) << 10) - region_size),
                region_size);

        // End of block
        instance->setBitmap(GetPointer(((256 + 256) << 10) - region_size),
                region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x8000000000000201, bitmap[1]);
        EXPECT_EQ(0x0000000000000000, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetSmallRegionMidPage) {
        size_t region_size = 2048;

        // One cache-line after page head
        instance->setBitmap(GetPointer(((512 + 4) << 10) + 64),
                region_size);

        // One cache-line before page tail
        instance->setBitmap(GetPointer(((512 + 16) << 10) - region_size - 64),
                region_size);

        // Mid page
        instance->setBitmap(
                GetPointer(((512 + 20) << 10) + (4096 - region_size) / 2),
                region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x0000000000000000, bitmap[1]);
        EXPECT_EQ(0x000000000000002A, bitmap[2]);
        EXPECT_EQ(0x0000000000000000, bitmap[3]);
    }

    TEST_F(BitmapTestSuite, SetSmallRegionSplitBetweenTwoPages) {
        size_t region_size = 256;

        // Two cache-lines before and two cache-lines after page limit
        instance->setBitmap(GetPointer(((256 + 4) << 10) - 128), region_size);

        // One cache-line before and three cache-lines after page limit
        instance->setBitmap(GetPointer(((256 + 16) << 10) - 64), region_size);

        // Three cache-lines before and one cache-line after page limit
        instance->setBitmap(GetPointer(((256 + 28) << 10) - 192), region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x00000000000000DB, bitmap[1]);
        EXPECT_EQ(0x0000000000000000, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetOnePage) {
        size_t region_size = GlobalAlloc::BitmapGranularity;

        // Half page before and half page after page limit
        instance->setBitmap(GetPointer(((256 + 4) << 10) + region_size / 2),
                region_size);

        // One cache-line before page limit
        instance->setBitmap(GetPointer(((256 + 20) << 10) - 64), region_size);

        // One cache-line after page limit
        instance->setBitmap(GetPointer(((256 + 28) << 10) + 64), region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x00000000000001B6, bitmap[1]);
        EXPECT_EQ(0x0000000000000000, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetOnePageAligned) {
        size_t region_size = GlobalAlloc::BitmapGranularity;

        // First page in block
        instance->setBitmap(GetPointer(512 << 10), region_size);

        // Last page in block
        instance->setBitmap(GetPointer((512 + 256 - 4) << 10), region_size);

        // Mid page in block
        instance->setBitmap(GetPointer((256 + 128) << 10), region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x0000000100000000, bitmap[1]);
        EXPECT_EQ(0x8000000000000001, bitmap[2]);
        EXPECT_EQ(0x0000000000000000, bitmap[3]);
    }

    TEST_F(BitmapTestSuite, SetMultiPage) {
        size_t region_size = GlobalAlloc::BitmapGranularity * 3;

        // Half page before and half page after page limit
        instance->setBitmap(GetPointer((256 << 10) + region_size / 2),
                region_size);

        // One cache-line before page limit
        instance->setBitmap(GetPointer(((256 + 24) << 10) - 64),
                region_size);

        // One cache-line after page limit
        instance->setBitmap(GetPointer(((256 + 40) << 10) + 64),
                region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x0000000000003DFE, bitmap[1]);
        EXPECT_EQ(0x0000000000000000, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetMultiPageAligned) {
        size_t region_size = GlobalAlloc::BitmapGranularity * 4;

        // First pages of the block
        instance->setBitmap(GetPointer(256 << 10), region_size);
        instance->setBitmap(GetPointer((256 + 20) << 10), region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x0000000000000000, bitmap[0]);
        EXPECT_EQ(0x00000000000001EF, bitmap[1]);
        EXPECT_EQ(0x0000000000000000, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetMultiCell) {
        size_t region_size = GlobalAlloc::BitmapGranularity * 2;

        // Start half page before block limit
        instance->setBitmap(GetPointer((256 << 10) - 2048), region_size);

        // End half page after block limit
        instance->setBitmap(GetPointer((512 << 10) - (region_size - 2048)),
                region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x8000000000000000, bitmap[0]);
        EXPECT_EQ(0xC000000000000003, bitmap[1]);
        EXPECT_EQ(0x0000000000000001, bitmap[2]);
    }

    TEST_F(BitmapTestSuite, SetMultiCellAligned) {
        size_t region_size = GlobalAlloc::BitmapGranularity * 3;

        // One page before block limit
        instance->setBitmap(GetPointer((256 - 4) << 10), region_size);

        // One page after block limit
        instance->setBitmap(GetPointer(((512 + 4) << 10) - region_size),
                region_size);

        // Verify
        SyncBitmaps();
        EXPECT_EQ(0x8000000000000000, bitmap[0]);
        EXPECT_EQ(0xC000000000000003, bitmap[1]);
        EXPECT_EQ(0x0000000000000001, bitmap[2]);
    }

    // Test unset bitmap
    TEST_F(BitmapTestSuite, FreeSmallRegionsFromPage) {
        size_t region_size = 1024 - sizeof(chunk_header_t);

        SetUpAlloc();
        void *ptrs[5] = {
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            alloc->alloc(region_size)
        };

        // Verify allocation layout
        EXPECT_EQ((uintptr_t)ptrs[4] % 0x1000, sizeof(chunk_header_t));
        EXPECT_EQ((uintptr_t)ptrs[0] % 0x1000, sizeof(chunk_header_t));

        // Verify bitmap before free
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[4], 0x8000000000000000);

        // Free second region of last page (mid page)
        alloc->dealloc(ptrs[3]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[4], 0x8000000000000000);

        // Free first region of the last page (page head)
        alloc->dealloc(ptrs[4]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[4], 0x8000000000000000);

        // Free last region of the last page (page tail)
        alloc->dealloc(ptrs[2]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[4], 0x8000000000000000);
    }

    TEST_F(BitmapTestSuite, FreeSmallRegionOccupyingTwoPages) {
        size_t region_size = 3 * 1024 - sizeof(chunk_header_t);

        SetUpAlloc();
        void *ptrs[6] = {
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            alloc->alloc(region_size),
            NULL
        };

        // Verify allocation layout
        EXPECT_EQ((uintptr_t)ptrs[4] % 0x1000, sizeof(chunk_header_t));
        EXPECT_EQ((uintptr_t)ptrs[0] % 0x1000, sizeof(chunk_header_t));

        // Verify bitmap before free
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[4], 0xE000000000000000);

        // Two pages in one block
        alloc->dealloc(ptrs[2]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[2], 0xE000000000000000);

        // Two pages in different blocks
        EXPECT_EQ(alloc->alloc(region_size), ptrs[2]);

        int64_t block_capacity = GlobalAlloc::BitmapGranularity * 64;
        block_capacity -= 4 * region_size;
        while (block_capacity > 0) {
            ptrs[5] = alloc->alloc(region_size);
            block_capacity -= region_size;
        }
        alloc->alloc(region_size);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[5], 0xC000000000000000);
        alloc->dealloc(ptrs[5]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[5], 0xC000000000000000);
    }

    TEST_F(BitmapTestSuite, FreePageAligned) {
        size_t region_size = 4096;
        SetUpAlloc();
        void *ptrs[5] = {
            // Single page
            alloc->alloc(region_size - sizeof(chunk_header_t)),
            alloc->alloc(region_size - sizeof(chunk_header_t)),
            // Double page
            alloc->alloc(region_size * 2 - sizeof(chunk_header_t)),
            // Triple page
            alloc->alloc(region_size * 3 - sizeof(chunk_header_t)),
            // Quad page
            alloc->alloc(region_size * 4 - sizeof(chunk_header_t)),
        };

        // Verify allocation layout
        for (size_t i = 0; i < 5; i++) {
            EXPECT_EQ((uintptr_t)ptrs[i] % 0x1000, sizeof(chunk_header_t));
        }

        // Verify bitmap before free
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[4], 0xFFC0000000000000);

        // Free and verify bitmap
        alloc->dealloc(ptrs[1]); // One page
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0x7FC0000000000000);
        alloc->dealloc(ptrs[2]); // Two pages
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[2], 0x1FC0000000000000);
        alloc->dealloc(ptrs[3]); // Three pages
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[3], 0x03C0000000000000);
        alloc->dealloc(ptrs[4]); // Four pages
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[4], 0x0000000000000000);
        alloc->dealloc(ptrs[0]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000000);
    }

    TEST_F(BitmapTestSuite, FreePageOccupyingTwoBlocks) {

        size_t pages_per_block = 64;
        size_t page_size = 4096;

        SetUpAlloc();

        // Initialize allocation tables
        uintptr_t *aligned_ptrs = (uintptr_t *)malloc(sizeof(uintptr_t) *
                (pages_per_block + 1));
        for (size_t i = 0; i < pages_per_block; i++) {
            aligned_ptrs[i] =
                (uintptr_t)alloc->alloc(page_size - sizeof(chunk_header_t));
        }
        aligned_ptrs[pages_per_block] =
            (uintptr_t)alloc->alloc(page_size * 2 - sizeof(chunk_header_t));
        SyncBitmaps();
        CheckBitmap(aligned_ptrs[0], 0x0000000000000001);
        CheckBitmap(aligned_ptrs[1], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(aligned_ptrs[pages_per_block], 0x8000000000000000);

        uintptr_t *ptrs = (uintptr_t *)malloc(sizeof(uintptr_t) *
                (pages_per_block - 1));
        ptrs[0] = (uintptr_t)alloc->alloc(page_size + 2048 -
                sizeof(chunk_header_t));
        for (size_t i = 1; i < pages_per_block - 1; i++) {
            ptrs[i] = (uintptr_t)alloc->alloc(page_size -
                    sizeof(chunk_header_t));
        }
        SyncBitmaps();
        CheckBitmap(ptrs[0], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptrs[pages_per_block - 2], 0x8000000000000000);

        // Aligned
        alloc->dealloc((void *)aligned_ptrs[pages_per_block]);
        SyncBitmaps();
        CheckBitmap(aligned_ptrs[pages_per_block], 0x7FFFFFFFFFFFFFFF);

        // Not-aligned
        alloc->dealloc((void *)ptrs[pages_per_block - 2]);
        SyncBitmaps();
        CheckBitmap(ptrs[pages_per_block - 2], 0x0000000000000000);
        CheckBitmap(ptrs[pages_per_block - 3], 0x7FFFFFFFFFFFFFFF);

        free(aligned_ptrs);
        free(ptrs);
    }

    TEST_F(BitmapTestSuite, FreeLastSmallRegionFromMidPage) {
        size_t regions_per_page = 8;
        assert(regions_per_page > 4 && regions_per_page % 2 == 0);
        size_t region_size = (4096 / regions_per_page) - sizeof(chunk_header_t);

        // Initialize allocation tables
        SetUpAlloc();
        void **ptrs = (void **)malloc(sizeof(void *) * (regions_per_page + 2));
        for (size_t i = 0; i < regions_per_page + 2; i++) {
            ptrs[i] = alloc->alloc(region_size);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[1], 0xC000000000000000);

        // Free all regions from the last page except for on from the middle
        size_t index_to_keep = 2;
        for (size_t i = 1; i < regions_per_page + 1; i++) {
            if (i != index_to_keep) {
                alloc->dealloc(ptrs[i]);
            }
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xC000000000000000);

        // Free last region from the last page
        alloc->dealloc(ptrs[index_to_keep]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0x4000000000000000);
    }

    TEST_F(BitmapTestSuite, FreeLastSmallRegionFromPageHeadAndTail) {
        SetUpAlloc();
        size_t regions_per_page = 8;
        assert(regions_per_page > 4 && regions_per_page % 2 == 0);
        size_t region_size = (4096 / regions_per_page) - sizeof(chunk_header_t);

        // Initialize allocation tables
        void **ptrs = (void **)malloc(sizeof(void *) * (regions_per_page * 3 + 1));
        for (size_t i = 0; i < regions_per_page * 3 + 1; i++) {
            ptrs[i] = alloc->alloc(region_size);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[1], 0xE000000000000000);

        // Free all regions except for the tail region (last page)
        for (size_t i = 1; i < regions_per_page; i++) {
            alloc->dealloc(ptrs[1 + i]);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xE000000000000000);

        // Free all regions except for the head region (third from last)
        for (size_t i = 0; i < regions_per_page - 1; i++) {
            alloc->dealloc(ptrs[2 * regions_per_page + i + 1]);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xE000000000000000);

        // Free the tail region from last page
        alloc->dealloc(ptrs[1]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0x6000000000000000);

        // Free the head region from the third page from last
        alloc->dealloc(ptrs[regions_per_page * 3]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0x4000000000000000);
    }

    TEST_F(BitmapTestSuite, FreeLastSmallRegionOccupyingTwoPages) {

        size_t region_size = 2048 + 1024 - sizeof(chunk_header_t);
        size_t block_size = GlobalAlloc::BitmapGranularity * 64;
        size_t regions_per_block = block_size / region_size;

        // Initialize allocation tables
        SetUpAlloc();
        size_t regions = regions_per_block + 2;
        void **ptrs = (void **)malloc(sizeof(void *) * regions);
        for (size_t i = 0; i < regions; i++) {
            ptrs[i] = alloc->alloc(region_size);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x8000000000000000);

        // Same block
        alloc->dealloc(ptrs[1]);
        alloc->dealloc(ptrs[3]);
        alloc->dealloc(ptrs[4]);
        alloc->dealloc(ptrs[regions - 2]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xDFFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x8000000000000000);

        // Two blocks
        alloc->dealloc(ptrs[regions - 1]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x0);
        CheckBitmap((uintptr_t)ptrs[1], 0xDFFFFFFFFFFFFFFE);
    }

    TEST_F(BitmapTestSuite, FreeSmallRegionAndCoalesceWithRightRegion) {
        size_t region_size = 512 + 256 - sizeof(chunk_header_t);
        size_t block_size = GlobalAlloc::BitmapGranularity * 64;
        size_t regions = block_size / (region_size + sizeof(chunk_header_t)) + 3;

        // Initialize allocation tables
        SetUpAlloc();
        void **ptrs = (void **)malloc(sizeof(void *) * regions);
        for (size_t i = 0; i < regions; i++) {
            ptrs[i] = alloc->alloc(region_size);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x8000000000000000);

        // Same page
        for (size_t i = 1; i < 15; i++) {
            alloc->dealloc(ptrs[i]);
        }
        alloc->dealloc(ptrs[16]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0x3FFFFFFFFFFFFFFF);
        alloc->dealloc(ptrs[15]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0x1FFFFFFFFFFFFFFF);

        // Different pages
        for (size_t i = 0; i < 5; i++) {
            alloc->dealloc(ptrs[17 + i]);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[16], 0x1FFFFFFFFFFFFFFF);
        alloc->dealloc(ptrs[22]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[16], 0x0FFFFFFFFFFFFFFF);

        // Different blocks
        for (size_t i = 0; i < 5; i++) {
            alloc->dealloc(ptrs[regions - 3 - i]);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[16], 0x0FFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x8000000000000000);
        alloc->dealloc(ptrs[regions - 2]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[16], 0x0FFFFFFFFFFFFFFE);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x8000000000000000);
    }

    TEST_F(BitmapTestSuite, FreeSmallRegionAndCoalesceWithLeftRegion) {
        size_t region_size = 512 + 256 - sizeof(chunk_header_t);
        size_t block_size = GlobalAlloc::BitmapGranularity * 64;
        size_t regions = block_size / (region_size + sizeof(chunk_header_t)) + 2;

        // Initialize allocation tables
        SetUpAlloc();
        void **ptrs = (void **)malloc(sizeof(void *) * regions);
        for (size_t i = 0; i < regions; i++) {
            ptrs[i] = alloc->alloc(region_size);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x8000000000000000);

        // Different blocks
        alloc->dealloc(ptrs[regions - 1]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFF);
        CheckBitmap((uintptr_t)ptrs[regions - 1], 0x0000000000000000);

        // Same page
        for (size_t i = 0; i < 4; i++) {
            alloc->dealloc(ptrs[regions - i - 2]);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFF);
        alloc->dealloc(ptrs[regions - 6]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFE);

        // Different pages
        for (size_t i = 0; i < 5; i++) {
            alloc->dealloc(ptrs[7 + i]);
        }
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xFFFFFFFFFFFFFFFE);
        alloc->dealloc(ptrs[6]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[1], 0xBFFFFFFFFFFFFFFE);
    }

    // Test bitmap save and load
    TEST_F(BitmapTestSuite, LoadAndSaveBitmap) {
        size_t bitmap_cells = instance->bitmapSize() / sizeof(uint64_t);
        for (size_t i = 0; i < bitmap_cells; i++) {
            bitmap[i] = (uint64_t)rand();
        }
        instance->loadBitmap((const char *)bitmap);

        char *temp = (char *)malloc(instance->bitmapSize());
        instance->saveBitmap(temp);

        EXPECT_EQ(memcmp(bitmap, temp, instance->bitmapSize()), 0);
        free(temp);
    }

    TEST_F(BitmapTestSuite, ReallocAlreadyBigEnough) {
        const size_t increase = 32; // Must be less than 64 (cache-line)
        const size_t pageSize = GlobalAlloc::BitmapGranularity;

        const size_t oldSize = pageSize - sizeof(chunk_header_t) - increase;
        const size_t newSize = oldSize + increase;

        SetUpAlloc();
        void *ptrs[3] = {
            alloc->alloc(oldSize),
            alloc->alloc(oldSize),
            alloc->alloc(oldSize)
        };
        alloc->dealloc(ptrs[1]);

        // Verify setup
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[2], 0x4000000000000000);

        // Reallocate and check bitmaps
        alloc->realloc(ptrs[2], newSize);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[2], 0x4000000000000000);
    }

    TEST_F(BitmapTestSuite, ReallocFreeAndAlloc) {
        const size_t pageSize = GlobalAlloc::BitmapGranularity;

        const size_t oldSize = pageSize - sizeof(chunk_header_t);
        const size_t newSize = oldSize + pageSize;

        SetUpAlloc();
        void *ptrs[3] = {
            alloc->alloc(oldSize),
            alloc->alloc(oldSize),
            alloc->alloc(oldSize)
        };

        // Verify setup
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[2], 0xC000000000000000);

        // Reallocate and verify bitmaps
        alloc->realloc(ptrs[2], newSize);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);
        CheckBitmap((uintptr_t)ptrs[2], 0xB000000000000000);
    }

    TEST_F(BitmapTestSuite, ReallocExtendAndSplit) {
        const size_t pageSize = GlobalAlloc::BitmapGranularity;

        const size_t increase = pageSize / 2;
        const size_t oldSize = pageSize - sizeof(chunk_header_t);
        const size_t newSize = oldSize + increase;

        SetUpAlloc();
        void *ptrs[2] = {
            alloc->alloc(oldSize),
            NULL
        };

        // Verify setup
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000001);

        void *rPtr = alloc->realloc(ptrs[0], newSize);
        EXPECT_EQ(rPtr, ptrs[0]);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptrs[0], 0x0000000000000003);

        // Allocate all except for the remainder
        const size_t pagesPerBlock = FreeList::BlockSize / pageSize;
        const size_t remainderPages = pagesPerBlock - 2;
        for (size_t i = 0; i < remainderPages; i++) {
            void *t = alloc->alloc(pageSize - sizeof(chunk_header_t));
            EXPECT_NE(t, (void *)NULL);
        }

        // Make sure the remainder is usable
        ptrs[1] = alloc->alloc(pageSize - increase - sizeof(chunk_header_t));
        SyncBitmaps();

        uintptr_t ptr = (uintptr_t)ptrs[0];
        uint64_t step = FreeList::BlockSize / 8;
        CheckBitmap(ptr + 0 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 1 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 2 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 3 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 4 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 5 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 6 * step, 0xFFFFFFFFFFFFFFFF);
        CheckBitmap(ptr + 7 * step, 0xFFFFFFFFFFFFFFFF);

        ptr = (uintptr_t)alloc->alloc(64);
        SyncBitmaps();
        CheckBitmap(ptr, 0x0000000000000001);
    }

    TEST_F(BitmapTestSuite, ReallocExtendAndUseEntirety) {
        const size_t pageSize = GlobalAlloc::BitmapGranularity;

        const size_t oldSize = pageSize - sizeof(chunk_header_t);
        const size_t newSize = oldSize + pageSize;

        SetUpAlloc();
        void *ptr = alloc->alloc(oldSize);

        // Verify setup
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptr, 0x0000000000000001);

        // Reallocate and verify bitmap
        void *rPtr = alloc->realloc(ptr, newSize);
        EXPECT_EQ(rPtr, ptr);
        SyncBitmaps();
        CheckBitmap((uintptr_t)ptr, 0x0000000000000003);
    }

    TEST_F(BitmapTestSuite, BigAllocBitmapSetFullBlocks) {

        const size_t bitsPerBlock =
            FreeList::BlockSize / GlobalAlloc::BitmapGranularity;
        const size_t cellsPerBlock = bitsPerBlock / 64;
        const size_t blockCount = 3;
        const size_t allocSize = blockCount * FreeList::BlockSize;

        SetUpAlloc();
        void *ptr = alloc->alloc(allocSize - sizeof(chunk_header_t));
        EXPECT_NE((uintptr_t)ptr, 0);

        SyncBitmaps();

        uintptr_t p = (uintptr_t)ptr;
        for (size_t b = 0; b < blockCount; b++) {
            for (size_t c = 0; c < cellsPerBlock; c++) {
                CheckBitmap(p, UINT64_MAX);
                p += FreeList::BlockSize / cellsPerBlock;
            }
        }
        CheckBitmap(p, 0);
    }

    TEST_F(BitmapTestSuite, BigAllocBitmapSetPageAligned) {
        const size_t bitsPerBlock =
            FreeList::BlockSize / GlobalAlloc::BitmapGranularity;
        const size_t cellsPerBlock = bitsPerBlock / 64;
        const size_t allocSize = FreeList::BlockSize +
            GlobalAlloc::BitmapGranularity;

        SetUpAlloc();
        void *ptr = alloc->alloc(allocSize - sizeof(chunk_header_t));
        EXPECT_NE((uintptr_t)ptr, 0);

        SyncBitmaps();

        uintptr_t p = (uintptr_t)ptr;
        for (size_t i = 0; i < cellsPerBlock; i++) {
            CheckBitmap(p, UINT64_MAX);
            p += FreeList::BlockSize / cellsPerBlock;
        }
        CheckBitmap(p, 0x0000000000000001);
    }

    TEST_F(BitmapTestSuite, BigAllocBitmapSetUnaligned) {
        const size_t bitsPerBlock =
            FreeList::BlockSize / GlobalAlloc::BitmapGranularity;
        const size_t cellsPerBlock = bitsPerBlock / 64;
        const size_t allocSize = FreeList::BlockSize +
            GlobalAlloc::BitmapGranularity;

        SetUpAlloc();
        void *ptr = alloc->alloc(allocSize);
        EXPECT_NE((uintptr_t)ptr, 0);

        SyncBitmaps();

        uintptr_t p = (uintptr_t)ptr;
        for (size_t i = 0; i < cellsPerBlock; i++) {
            CheckBitmap(p, UINT64_MAX);
            p += FreeList::BlockSize / cellsPerBlock;
        }
        CheckBitmap(p, 0x0000000000000003);
    }

    TEST_F(BitmapTestSuite, BigDeallocBitmapUnsetFullBlocks) {
        const size_t bitsPerBlock =
            FreeList::BlockSize / GlobalAlloc::BitmapGranularity;
        const size_t cellsPerBlock = bitsPerBlock / 64;
        const size_t blockCount = 3;
        const size_t allocSize = blockCount * FreeList::BlockSize;

        SetUpAlloc();
        void *ptr0 = alloc->alloc(allocSize - sizeof(chunk_header_t));
        EXPECT_NE((uintptr_t)ptr0, 0);
        void *ptr1 = alloc->alloc(allocSize - sizeof(chunk_header_t));
        EXPECT_NE((uintptr_t)ptr1, 0);

        alloc->dealloc(ptr0);

        SyncBitmaps();

        uintptr_t p0 = (uintptr_t)ptr0;
        for (size_t b = 0; b < blockCount; b++) {
            for (size_t c = 0; c < cellsPerBlock; c++) {
                CheckBitmap(p0, 0);
                p0 += FreeList::BlockSize / cellsPerBlock;
            }
        }

        uintptr_t p1 = (uintptr_t)ptr1;
        for (size_t b = 0; b < blockCount; b++) {
            for (size_t c = 0; c < cellsPerBlock; c++) {
                CheckBitmap(p1, UINT64_MAX);
                p1 += FreeList::BlockSize / cellsPerBlock;
            }
        }
    }

    TEST_F(BitmapTestSuite, BigDeallocBitmapUnsetPageAligned) {
        const size_t bitsPerBlock =
            FreeList::BlockSize / GlobalAlloc::BitmapGranularity;
        const size_t cellsPerBlock = bitsPerBlock / 64;
        const size_t allocSize = FreeList::BlockSize +
            GlobalAlloc::BitmapGranularity * 8;
        assert(allocSize % FreeList::BlockSize != 0);

        SetUpAlloc();
        void *ptr = alloc->alloc(allocSize - sizeof(chunk_header_t));
        EXPECT_NE((uintptr_t)ptr, 0);

        alloc->dealloc(ptr);

        SyncBitmaps();

        uintptr_t p = (uintptr_t)ptr;
        for (size_t c = 0; c < cellsPerBlock; c++) {
            CheckBitmap(p, 0);
            p += FreeList::BlockSize / cellsPerBlock;
        }
        CheckBitmap(p, 0);
    }

    TEST_F(BitmapTestSuite, BigDeallocBitmapUnsetUnaligned) {
        const size_t bitsPerBlock =
            FreeList::BlockSize / GlobalAlloc::BitmapGranularity;
        const size_t cellsPerBlock = bitsPerBlock / 64;
        const size_t allocSize = FreeList::BlockSize +
            GlobalAlloc::BitmapGranularity * 4;

        SetUpAlloc();
        void *ptr = alloc->alloc(allocSize);
        EXPECT_NE((uintptr_t)ptr, 0);
        alloc->dealloc(ptr);

        SyncBitmaps();

        uintptr_t p = (uintptr_t)ptr;
        for (size_t i = 0; i < cellsPerBlock; i++) {
            CheckBitmap(p, 0);
            p += 64 * GlobalAlloc::BitmapGranularity;
        }
        CheckBitmap(p, 0);
    }
}
