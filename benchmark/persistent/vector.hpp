#include "../common/base.hpp"
#include "../../src/savitar.hpp"
#include <uuid/uuid.h>
#include <vector>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

class PersistentVector : public PersistentObject {
public:
    typedef char * T;

    PersistentVector(uuid_t id) : PersistentObject(id) {
        void *t = alloc->alloc(sizeof(vector<T, STLAlloc<T>>));
        vector<T, STLAlloc<T>> *obj = (vector<T, STLAlloc<T>> *)t;
        v_vector = new(obj) vector<T, STLAlloc<T>>((STLAlloc<T>(alloc)));
    }

    void push_back(T value) {
        // <compiler>
        Savitar_thread_notify(3, this, PushBackTag, value);
        // </compiler>
        v_vector->push_back(value);
        // <compiler>
        Savitar_thread_wait(this, this->log);
        // </compiler>
    }

    // <compiler>
    PersistentVector() : PersistentObject(true) {}

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(PersistentVector));
        PersistentVector *obj = (PersistentVector *)temp;
        PersistentVector *object = new (obj) PersistentVector(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static PersistentVector *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        PersistentVector *obj = (PersistentVector *)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<PersistentVector *>(BaseFactory(id));
            manager.createNew(classID(), obj);
        }
        manager.unlock();
        return obj;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[4]; // Max arguments of the class

        switch (tag) {
            case PushBackTag:
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
            case PushBackTag:
                {
                char *value = (char *)args;
                if (!dry) push_back(value);
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

    static uint64_t classID() { return 1; }
    // </compiler>

private:
    vector<T, STLAlloc<T>> *v_vector;
    // <compiler>
    enum MethodTags {
        PushBackTag = 1,
    };
    // </compiler>
};

class NvVector : public Benchmark {
public:
    NvVector(Configuration *cfg) : Benchmark(cfg) {  }

    virtual void init(unsigned int myID) {
        uuid_t uuid;
        uuid_generate(uuid);
        PersistentVector *v = PersistentVector::Factory(uuid);
        objects[myID] = (uintptr_t)v;
    }

    virtual void cleanup(unsigned int myID) {
        // Nothing to do
    }

    virtual unsigned int worker(unsigned int myID) {
        PersistentVector *v = (PersistentVector *)objects[myID];
        ObjectAlloc *alloc = v->getAllocator();

        unsigned int i = 0;
        for (i = 0; i < traces[myID]->size(); i++) {
            char *buffer = (char *)alloc->alloc(cfg->valueSize);
            memcpy(buffer, valueBuffer, cfg->valueSize);
            v->push_back(buffer);
        }
        return i;
    }

    void printReport() {
        cout << "persistent-vector,";
        Benchmark::printReport();
    }
};
