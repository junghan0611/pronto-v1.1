#include "../common/base.hpp"
#include "../../src/savitar.hpp"
#include <uuid/uuid.h>
#include <queue>
#include <iostream>
#include <string.h>

using namespace std;

class PersistentPriorityQueue : public PersistentObject {
public:
    typedef char * T;
    typedef std::vector<T, STLAlloc<T>> VectorType;
    typedef std::priority_queue<T, VectorType, std::less<T>> QueueType;

    PersistentPriorityQueue(uuid_t id) : PersistentObject(id) {

        // Container
        void *t = alloc->alloc(sizeof(VectorType));
        VectorType *v = (VectorType *)t;
        v = new(v) VectorType((STLAlloc<T>(alloc)));

        // Adapter
        t = alloc->alloc(sizeof(QueueType));
        QueueType *obj = (QueueType *)t;
        less<T> cmp;
        v_queue = new(obj) QueueType(cmp, *v);
    }

    void push(T value) {
        // <compiler>
        Savitar_thread_notify(3, this, PushTag, value);
        // </compiler>
        v_queue->push(value);
        // <compiler>
        Savitar_thread_wait(this, this->log);
        // </compiler>
    }

    // <compiler>
    PersistentPriorityQueue() : PersistentObject(true) { }

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(PersistentPriorityQueue));
        PersistentPriorityQueue *obj = (PersistentPriorityQueue *)temp;
        PersistentPriorityQueue *object = new (obj) PersistentPriorityQueue(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static PersistentPriorityQueue *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        PersistentPriorityQueue *obj = (PersistentPriorityQueue *)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<PersistentPriorityQueue *>(BaseFactory(id));
            manager.createNew(classID(), obj);
        }
        manager.unlock();
        return obj;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[4]; // Max arguments of the class

        switch (tag) {
            case PushTag:
                {
                vector[0].addr = &tag;
                vector[0].len = sizeof(tag);
                vector[1].addr = (void *)args[0];
                vector[1].len = strlen((char *)args[0]) + 1;
                vector_size = 2;
                }
                break;
            default:
                assert(false);
                break;
        }

        return AppendLog(vector, vector_size);
    }

    size_t Play(uint64_t tag, uint64_t *args, bool dry) {
        size_t bytes_processed = 0;
        switch (tag) {
            case PushTag:
                {
                char *value = (char *)args;
                if (!dry) push(value);
                bytes_processed = strlen(value) + 1;
                }
                break;
            default:
                {
                PRINT("Unknown tag: %zu\n", tag);
                assert(false);
                }
                break;
        }
        return bytes_processed;
    }

    static uint64_t classID() { return 3; }
    // </compiler>

private:
    QueueType *v_queue;
    // <compiler>
    enum MethodTags {
        PushTag = 1,
    };
    // </compiler>
};

class NvPriorityQueue : public Benchmark {
public:
    NvPriorityQueue(Configuration *cfg) : Benchmark(cfg) {  }

    virtual void init(unsigned int myID) {
        uuid_t uuid;
        uuid_generate(uuid);
        PersistentPriorityQueue *q = PersistentPriorityQueue::Factory(uuid);
        objects[myID] = (uintptr_t)q;
    }

    virtual void cleanup(unsigned int myID) {
        // Nothing to do
    }

    virtual unsigned int worker(unsigned int myID) {
        PersistentPriorityQueue *q = (PersistentPriorityQueue *)objects[myID];
        ObjectAlloc *alloc = q->getAllocator();

        unsigned int i = 0;
        for (i = 0; i < traces[myID]->size(); i++) {
            char *buffer = (char *)alloc->alloc(cfg->valueSize);
            memcpy(buffer, valueBuffer, cfg->valueSize);
            q->push(buffer);
        }
        return i;
    }

    void printReport() {
        cout << "persistent-queue,";
        Benchmark::printReport();
    }
};
