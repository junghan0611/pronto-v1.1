#include <signal.h>
#include <fcntl.h>
#include <string>
#include <uuid/uuid.h>
#include <sys/mman.h>
#include <time.h>
#include "../../src/savitar.hpp"
#include "../../src/ckpt_alloc.hpp"

class RdWrObject : public PersistentObject {
public:
    RdWrObject(uuid_t id) : PersistentObject(id) { }

    RdWrObject() : PersistentObject(true) {}

    // TODO refactor and replace the allocator
    void allocateBlocks(size_t numberOfBlocks) {
        blocks = (uintptr_t *)malloc(sizeof(uintptr_t) * numberOfBlocks);
        for (size_t i = 0; i < numberOfBlocks; i++) {
            size_t sz = FreeList::BlockSize - sizeof(chunk_header_t);
            if (i == 0) sz -= (128 + 64); // space allocated for current object

            blocks[i] = (uintptr_t)alloc->alloc(sz);
        }

        // Swap first and last blocks
        uintptr_t temp = blocks[0];
        blocks[0] = blocks[numberOfBlocks - 1];
        blocks[numberOfBlocks - 1] = temp;
    }

    uint64_t read(off_t offset) {
        off_t blockIndex = getBlockIndex(offset);
        off_t blockOffset = getBlockOffset(offset);

        uint64_t *dataPtr = (uint64_t *)blocks[blockIndex];
        return dataPtr[blockOffset];
    }

    void write(off_t offset, uint64_t value) {
        off_t blockIndex = getBlockIndex(offset);
        off_t blockOffset = getBlockOffset(offset);
        uint64_t *dataPtr = (uint64_t *)blocks[blockIndex];

        Savitar_thread_notify(4, this, WriteTag, &offset, &value);
        dataPtr[blockOffset] = value;
        Savitar_thread_wait(this, this->log);
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        // We never recover this object
    }

    static RdWrObject *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        RdWrObject *temp = (RdWrObject *)alloc->alloc(sizeof(RdWrObject));
        RdWrObject *object = new(temp) RdWrObject(id);
        manager.createNew(classID(), object);
        manager.unlock();
        return object;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[4]; // Max arguments of the class

        switch (tag) {
            case WriteTag:
                {
                vector[0].addr = &tag;
                vector[0].len = sizeof(tag);
                vector[1].addr = (void *)args[0];
                vector[1].len = sizeof(off_t);
                vector[2].addr = (void *)args[1];
                vector[2].len = sizeof(uint64_t);
                vector_size = 3;
                }
                break;
            default:
                assert(false);
                break;
        }

        return AppendLog(vector, vector_size);
    }

    size_t Play(uint64_t tag, uint64_t *args, bool dry) {
        assert(false); // We never recover this object
    }

    static uint64_t classID() { return 1; }

    static_assert(sizeof(chunk_header_t) == sizeof(uint64_t),
            "Fix the number of cells per block!");
    const size_t CellsPerBlock = FreeList::BlockSize / sizeof(uint64_t) - 1;

protected:
    inline off_t getBlockIndex(off_t offset) {
        return offset / CellsPerBlock;
    }
    inline off_t getBlockOffset(off_t offset) {
        return offset % CellsPerBlock;
    }

private:
    uintptr_t *blocks;
    enum MethodTags {
        WriteTag = 1,
    };
};

static_assert((sizeof(RdWrObject) + sizeof(chunk_header_t) <= 128 + 64) &&
              (sizeof(RdWrObject) + sizeof(chunk_header_t) > 128),
        "Memory alignment failure: fix RdWrObject allocation method.");

class RecoveryOverhead {
public:
    RecoveryOverhead() {
        numberOfPages = 1;
        writeRatio = 50;
        operationCount = 1000000;
        randomAccess = false;
    }

    void setNumberOfPages(size_t num) { numberOfPages = num; }
    void setWriteRatio(unsigned ratio) { writeRatio = ratio; }
    void setOperationCount(size_t count) { operationCount = count; }
    void setRandomAccess(bool flag) { randomAccess = flag; }

    static void *workerMethod(void *arg) {
        RecoveryOverhead *instance = (RecoveryOverhead *)arg;
        RdWrObject *object = instance->object;

        const size_t numOfPages = instance->numberOfPages;
        const size_t cellsPerBlk = object->CellsPerBlock;
        const size_t resvdCells = (128 + 64) / sizeof(uint64_t);
        const size_t totalCells = cellsPerBlk * numOfPages - resvdCells;

        for (size_t i = 0; i < instance->operationCount; i++) {
            off_t offset = i % totalCells;
            if (instance->randomAccess) offset = rand() % totalCells;

            if (rand() % 100 >= instance->writeRatio) {
                object->read(offset);
            }
            else {
                uint64_t value = rand();
                object->write(offset, value);
            }
        }

        return NULL;
    }

    static int mainMethod(int argc, char **argv) {
        RecoveryOverhead *instance = (RecoveryOverhead *)argv;
        struct timespec start, end;

        uuid_t id;
        uuid_generate(id);
        RdWrObject *object = RdWrObject::Factory(id);
        object->allocateBlocks(instance->numberOfPages);
        instance->object = object;

        pthread_t thread;

        // No snapshot
        clock_gettime(CLOCK_REALTIME, &start);
        Savitar_thread_create(&thread, NULL, workerMethod, instance);
        pthread_join(thread, NULL);
        clock_gettime(CLOCK_REALTIME, &end);
        instance->t1 = start;
        instance->t2 = end;

        // With snapshot
        clock_gettime(CLOCK_REALTIME, &start);
        Savitar_thread_create(&thread, NULL, workerMethod, instance);
        raise(SIGUSR1); // triggers snapshot creation
        pthread_join(thread, NULL);
        clock_gettime(CLOCK_REALTIME, &end);
        instance->t3 = start;
        instance->t4 = end;

        return 0;
    }

    void run() {
        PersistentFactory::registerFactory<RdWrObject>();
        int s = Savitar_main(mainMethod, 0, (char **)this);
        assert(s == 0);
    }

    uint64_t getExecTimeWithNoSnapshot() {
        uint64_t execTime = (t2.tv_sec - t1.tv_sec) * 1E9;
        execTime += (t2.tv_nsec - t1.tv_nsec);
        return execTime;
    }

    uint64_t getExecTimeWithSnapshots() {
        uint64_t execTime = (t4.tv_sec - t3.tv_sec) * 1E9;
        execTime += (t4.tv_nsec - t3.tv_nsec);
        return execTime;
    }

private:
    RdWrObject *object;
    struct timespec t1, t2, t3, t4;
    size_t numberOfPages;
    unsigned writeRatio;
    size_t operationCount;
    bool randomAccess;
};
