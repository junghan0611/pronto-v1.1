#include "../src/snapshot.hpp"
#include "../src/savitar.hpp"
#include "gtest/gtest.h"
#include <limits.h>
#include <stdint.h>
#include <fcntl.h>

namespace {

    class SnapshotTestSuite : public testing::Test {
        protected:
            virtual void SetUp() {  }
            virtual void TearDown() {  }
            void removeSnapshot(uint64_t id = 1) {
                std::string snapshotPath = PMEM_PATH;
                snapshotPath += "/snapshot.";
                snapshotPath += std::to_string(id);
                remove(snapshotPath.c_str());
            }

        public:
            int getFileDescriptor(Snapshot *o) { return o->fd; }
            snapshot_header_t *getView(Snapshot *o) { return o->view; }
            uint64_t *getContext(Snapshot *o) { return o->context; }

            void prepareSnapshot(Snapshot *o) { o->prepareSnapshot(); }
            void cleanEnvironment(Snapshot *o) { o->cleanEnvironment(); }
            void nonTemporalCopy(Snapshot *o, char *dst, char *src) {
                o->nonTemporalPageCopy(dst, src);
            }
            void extendSnapshot(Snapshot *o, size_t blocks) {
                o->extendSnapshot(blocks);
            }
    };

    TEST_F(SnapshotTestSuite, Singleton) {
        EXPECT_EQ(Snapshot::anyActiveSnapshot(), false);
        Snapshot *ckpt = new Snapshot(PMEM_PATH);
        EXPECT_EQ(Snapshot::anyActiveSnapshot(), true);
        EXPECT_NE(Snapshot::getInstance(), nullptr);
        EXPECT_EQ(Snapshot::getInstance(), ckpt);
        delete ckpt;
    }

    TEST_F(SnapshotTestSuite, PrepareAndCleanEnvironment) {
        Snapshot *ckpt = new Snapshot(PMEM_PATH);
        EXPECT_EQ(getView(ckpt), nullptr);
        EXPECT_EQ(getContext(ckpt), nullptr);
        EXPECT_EQ(getFileDescriptor(ckpt), 0);

        prepareSnapshot(ckpt);
        EXPECT_NE(getView(ckpt), nullptr);
        EXPECT_NE(getContext(ckpt), nullptr);
        EXPECT_NE(getFileDescriptor(ckpt), 0);

        size_t snapshotSize = sizeof(snapshot_header_t);
        snapshotSize += GlobalAlloc::snapshotSize();
        snapshotSize += GlobalAlloc::getInstance()->bitmapSize();
        if (snapshotSize % 64 != 0) snapshotSize += 64 - (snapshotSize % 64);
        snapshot_header_t *header = getView(ckpt);
        EXPECT_EQ(header->identifier, 1);
        EXPECT_EQ(header->time, 0);
        EXPECT_EQ(header->size, snapshotSize);
        EXPECT_EQ(header->object_count, 0);
        EXPECT_EQ(header->bitmap_offset, sizeof(snapshot_header_t));
        EXPECT_EQ(header->global_offset, sizeof(snapshot_header_t) +
                GlobalAlloc::getInstance()->bitmapSize());
        EXPECT_EQ(header->data_offset, snapshotSize);
        EXPECT_EQ(header->alloc_offset, header->global_offset +
                GlobalAlloc::getInstance()->snapshotSize());

        cleanEnvironment(ckpt);
        EXPECT_EQ(getView(ckpt), nullptr);
        EXPECT_EQ(getContext(ckpt), nullptr);
        EXPECT_EQ(getFileDescriptor(ckpt), 0);

        removeSnapshot();
        delete ckpt;
    }

    TEST_F(SnapshotTestSuite, NonTemporalPageCopy) {
        Snapshot *ckpt = new Snapshot(PMEM_PATH);
        const size_t pageSize = 4096;
        const size_t pageCount = 32;

        char *src = (char *)malloc(pageSize * pageCount);
        char *dst = (char *)malloc(pageSize * pageCount);

        for (size_t i = 0; i < pageCount; i++) {
            for (size_t j = 0; j < pageSize; j++) {
                src[i * pageSize + j] = (char)(rand() % 255);
            }
        }

        for (size_t i = 0; i < pageCount; i++) {
            nonTemporalCopy(ckpt, dst, src);
            for (size_t j = 0; j < pageSize; j++) EXPECT_EQ(src[j], dst[j]);
        }
        delete ckpt;
    }

    TEST_F(SnapshotTestSuite, NonTemporalPageCopyOddPages) {
        Snapshot *ckpt = new Snapshot(PMEM_PATH);
        const size_t pageSize = GlobalAlloc::BitmapGranularity;
        const size_t pageCount = FreeList::BlockSize / pageSize;

        char *src = (char *)malloc(pageSize * pageCount);
        char *dst = (char *)malloc(pageSize * pageCount);
        memset(dst, 0, pageSize * pageCount);

        // Set data
        for (size_t i = 0; i < pageCount; i++) {
            for (size_t j = 0; j < pageSize; j++) {
                src[i * pageSize + j] = (char)(rand() % 255);
            }
        }

        // Copy odd pages
        for (size_t i = 0; i < pageCount; i++) {
            if (i % 2 == 1) {
                nonTemporalCopy(ckpt, &dst[i * pageSize], &src[i * pageSize]);
            }
        }

        // Verify data
        for (size_t i = 0; i < pageCount; i++) {
            if (i % 2 == 0) { // Must be zero
                for (size_t j = 0; j < pageSize; j++) {
                    EXPECT_EQ(dst[i * pageSize + j], 0);
                }
            }
            else { // Must be random data
                for (size_t j = 0; j < pageSize; j++) {
                    EXPECT_EQ(dst[i * pageSize + j], src[i * pageSize + j]);
                }
            }
        }

        delete ckpt;
    }

    TEST_F(SnapshotTestSuite, ExtendSnapshot) {
        Snapshot *ckpt = new Snapshot(PMEM_PATH);
        prepareSnapshot(ckpt);
        size_t oldSize = getView(ckpt)->size;
        size_t blocks = 1 + rand() % 10;
        extendSnapshot(ckpt, blocks);
        size_t newSize = getView(ckpt)->size;
        EXPECT_EQ(newSize, oldSize + blocks * FreeList::BlockSize);
        delete ckpt;
        removeSnapshot();
    }

    TEST_F(SnapshotTestSuite, PageFaultHandler) {
        Snapshot *ckpt = new Snapshot(PMEM_PATH);

        void *base = (void *)GlobalAlloc::BaseAddress;
        size_t PPBlk = FreeList::BlockSize / GlobalAlloc::BitmapGranularity;

        // Populate data
        GlobalAlloc::getInstance(); // Forces allocation of MinPoolSize bytes
        char *ptr = (char *)base;
        for (size_t i = 0; i < PPBlk; i++) {
            memset(ptr, i % 255, GlobalAlloc::BitmapGranularity);
            ptr += GlobalAlloc::BitmapGranularity;
        }

        // Simulate saved bitmap
        prepareSnapshot(ckpt);
        uint64_t *bitmap = (uint64_t *)((char *)getView(ckpt) +
                getView(ckpt)->bitmap_offset);
        uint64_t *context = getContext(ckpt);
        for (size_t i = 0; i < PPBlk / 64; i++) {
            bitmap[i] = 0x8080808080808080;
        }
        context[0] = Snapshot::getInstance()->UsedHugePage;

        // Extend snapshot by one block
        size_t oldSize = getView(ckpt)->size;
        extendSnapshot(ckpt, 1);
        EXPECT_EQ(getView(ckpt)->size, oldSize + FreeList::BlockSize);
        char *dataPtr = (char *)getView(ckpt) + getView(ckpt)->data_offset;
        memset(dataPtr, 0, FreeList::BlockSize);

        // Verify data region
        for (size_t i = 0; i < PPBlk; i++) {
            for (size_t b = 0; b < GlobalAlloc::BitmapGranularity; b++) {
                EXPECT_EQ(dataPtr[b], 0);
            }
            dataPtr += GlobalAlloc::BitmapGranularity;
        }

        // Simulate page-fault
        ckpt->pageFaultHandler((char *)base + rand() % FreeList::BlockSize);

        // Verify snapshot and context
        char expectedChar = 0;
        dataPtr = (char *)getView(ckpt) + getView(ckpt)->data_offset;
        for (size_t i = 0; i < PPBlk / 64; i++) {
            uint64_t bit = bitmap[i];
            for (size_t p = 0; p < 64; p++) {
                if (bit & 0x0000000000000001) {
                    for (size_t j = 0; j < GlobalAlloc::BitmapGranularity; j++)
                        EXPECT_EQ(dataPtr[j], expectedChar);
                }
                // TODO fix this
                /*else {
                    for (size_t j = 0; j < GlobalAlloc::BitmapGranularity; j++) {
                        EXPECT_EQ(dataPtr[j], 0);
                        if (dataPtr[j] != 0) break;
                    }
                }*/
                expectedChar = (expectedChar + 1) % 255;
                dataPtr += GlobalAlloc::BitmapGranularity;
                bit = bit >> 1;
            }
        }
        EXPECT_EQ(context[0], Snapshot::getInstance()->SavedHugePage);

        removeSnapshot();
        delete ckpt;
    }

    TEST_F(SnapshotTestSuite, MarkPagesReadOnly) {
        // TODO
    }

    TEST_F(SnapshotTestSuite, SnapshotWorker) {
        // TODO
    }

    TEST_F(SnapshotTestSuite, SaveAllocationTables) {
        // TODO
    }

    TEST_F(SnapshotTestSuite, SaveModifiedPages) {
        // TODO
    }

    TEST_F(SnapshotTestSuite, MapSnapshot) {
        // TODO
    }

    TEST_F(SnapshotTestSuite, LoadSnapshot) {
        // TODO
    }

    TEST_F(SnapshotTestSuite, Catalog) {
        Snapshot *ckpt = new Snapshot(PMEM_PATH);
        EXPECT_EQ(ckpt->lastSnapshotID(), 0);
        for (uint64_t i = 1; i <= 10; i++) {
            // Simulate snapshot
            experimental::filesystem::path snapshotPath = PMEM_PATH;
            snapshotPath /= "snapshot.";
            snapshotPath += std::to_string(i);;
            int fd = open(snapshotPath.c_str(), O_RDWR | O_CREAT | O_EXCL, 0666);
            assert(fd > 0);
            close(fd);
            EXPECT_EQ(ckpt->lastSnapshotID(), i);
        }
        delete ckpt;

        // Remove snapshots
        for (uint64_t i = 1; i <= 10; i++) removeSnapshot(i);
    }
}
