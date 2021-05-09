#include <signal.h>
#include <fcntl.h>
#include <string>
#include <uuid/uuid.h>
#include <sys/mman.h>
#include "../../src/savitar.hpp"
#include "../../src/snapshot.hpp"
#include "../../src/ckpt_alloc.hpp"

class DummyObject : public PersistentObject {
public:
    DummyObject(uuid_t id) : PersistentObject(id) {
        totalAllocated = 0;
    }

    DummyObject() : PersistentObject(true) {}

    void allocateAndFillOneBlock() {
        size_t sz = FreeList::BlockSize - sizeof(chunk_header_t);
        if (totalAllocated == 0) sz -= 64; // Space for current object
        void *block = alloc->alloc(sz);
        memset(block, (char)(rand() % 255), sz);
        totalAllocated++;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        // We never recover this object
    }

    static DummyObject *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        // We can never recovery this object, so we always create
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        DummyObject *temp = (DummyObject *)alloc->alloc(sizeof(DummyObject));
        DummyObject *object = new(temp) DummyObject(id);
        manager.createNew(classID(), object);
        manager.unlock();
        return object;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        assert(false); // We never log for this object
    }

    size_t Play(uint64_t tag, uint64_t *args, bool dry) {
        assert(false); // We never recover this object
    }

    static uint64_t classID() { return 1; }

private:
    size_t totalAllocated;
};

class RecoveryBreakdown {
public:
    RecoveryBreakdown(size_t pageCount) {
        modifiedPages = pageCount;
    }

    void run() {
        // Create snapshot
        PersistentFactory::registerFactory<DummyObject>();
        int s = Savitar_main(mainMethod, 0, (char **)this);
        assert(s == 0);

        // Read snapshot sync and async latencies
        uint32_t id = 1; // Assume there is only one snapshot
        snapshot_header_t *header = mapSnapshotHeader(id);
        syncLatency = header->sync_latency;
        asyncLatency = header->async_latency;
        unmapSnapshotHeader(header);
    }

    static int mainMethod(int argc, char **argv) {
        RecoveryBreakdown *instance = (RecoveryBreakdown *)argv;

        uuid_t id;
        uuid_generate(id);
        DummyObject *object = DummyObject::Factory(id);

        for (size_t i = 0; i < instance->modifiedPages; i++) {
            object->allocateAndFillOneBlock();
        }

        raise(SIGUSR1); // Create snapshot
        return 0;
    }

    uint32_t getSynchronousLatency() { return syncLatency; }
    uint32_t getAsynchronousLatency() { return asyncLatency; }

protected:
    snapshot_header_t *mapSnapshotHeader(uint32_t id) {

        snapshot_header_t *snapshot = NULL;
        std::string path = PMEM_PATH;
        path += "snapshot." + std::to_string(id);

        int fd = open(path.c_str(), O_RDONLY, 0666);
        assert(fd > 0);
        snapshot = (snapshot_header_t *)mmap(NULL, sizeof(snapshot_header_t),
                PROT_READ, MAP_SHARED, fd, 0);
        assert(snapshot != NULL);
        close(fd);
        return snapshot;
    }

    void unmapSnapshotHeader(snapshot_header_t *header) {
        munmap(header, sizeof(snapshot_header_t));
    }

private:
    size_t modifiedPages;
    uint32_t syncLatency; // ns
    uint32_t asyncLatency; // ns
};
