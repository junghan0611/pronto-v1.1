#pragma once
#include "../common/base.hpp"
#include "../../src/savitar.hpp"
#include <uuid/uuid.h>
#include <unordered_map>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

class PersistentMap : public PersistentObject {
public:
    typedef char * T;
    typedef unordered_map<T, T, hash<T>, equal_to<T>, STLAlloc<T>> MapType;

    PersistentMap(uuid_t id) : PersistentObject(id) {
        hash<T> hasher;
        equal_to<T> cmp;
        void *t = alloc->alloc(sizeof(MapType));
        MapType *obj = (MapType *)t;
        v_map = new(obj) MapType(16, hasher, cmp, STLAlloc<T>(alloc));
    }

    void insert(T key, T value) {
        // <compiler>
        Savitar_thread_notify(4, this, InsertTag, key, value);
        // </compiler>
        v_map->insert(make_pair(key, value));
        // <compiler>
        Savitar_thread_wait(this, this->log);
        // </compiler>
    }

    // <compiler>
    PersistentMap() : PersistentObject(true) { }

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(PersistentMap));
        PersistentMap *obj = (PersistentMap *)temp;
        PersistentMap *object = new (obj) PersistentMap(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static PersistentMap *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        PersistentMap *obj = (PersistentMap *)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<PersistentMap *>(BaseFactory(id));
            manager.createNew(classID(), obj);
        }
        manager.unlock();
        return obj;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[4]; // Max arguments of the class

        switch (tag) {
            case InsertTag:
                {
                vector[0].addr = &tag;
                vector[0].len = sizeof(tag);
                vector[1].addr = (void *)args[0];
                vector[1].len = strlen((char *)args[0]) + 1;
                vector[2].addr = (void *)args[1];
                vector[2].len = strlen((char *)args[1]) + 1;
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
        size_t bytes_processed = 0;
        switch (tag) {
            case InsertTag:
                {
                char *key = (char *)args;
                char *value = (char *)args + strlen(key) + 1;
                if (!dry) insert(key, value);
                bytes_processed = strlen(key) + strlen(value) + 2;
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

    static uint64_t classID() { return 2; }
    // </compiler>

private:
    MapType *v_map;
    // <compiler>
    enum MethodTags {
        InsertTag = 1,
    };
    // </compiler>
};

class NvMap : public Benchmark {
public:
    NvMap(Configuration *cfg) : Benchmark(cfg) {  }

    virtual void init(unsigned int myID) {
        uuid_t uuid;
        uuid_generate(uuid);
        PersistentMap *m = PersistentMap::Factory(uuid);
        objects[myID] = (uintptr_t)m;
    }

    virtual void cleanup(unsigned int myID) {
        // Nothing to do
    }

    virtual unsigned int worker(unsigned int myID) {
        PersistentMap *m = (PersistentMap *)objects[myID];
        ObjectAlloc *alloc = m->getAllocator();

        unsigned int i = 0;
        for (i = 0; i < traces[myID]->size(); i++) {

            // Create buffer for the value
            char *value = (char *)alloc->alloc(cfg->valueSize);
            memcpy(value, valueBuffer, cfg->valueSize);

            // Create buffer for the key
            char *key = (char *)alloc->alloc(traces[myID]->at(i).length());
            strcpy(key, traces[myID]->at(i).c_str());

            m->insert(key, value);
        }
        return i;
    }

    void printReport() {
        cout << "persistent-map,";
        Benchmark::printReport();
    }
};
