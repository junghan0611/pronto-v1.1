#pragma once
#ifdef MULTI_OBJECT
#include "unordered_map.hpp"

class NvHashMap : public Benchmark {
private:
    static const unsigned Buckets = 32;
    PersistentMap *pMaps[Buckets];
    mutex locks[Buckets];

public:
    NvHashMap(Configuration *cfg) : Benchmark(cfg) {
        for (unsigned b = 0; b < Buckets; b++) {
            uuid_t uuid;
            uuid_generate(uuid);
            pMaps[b] = PersistentMap::Factory(uuid);
            assert(pMaps[b] != NULL);
        }
    }

    virtual void init(unsigned int myID) {  }

    virtual void cleanup(unsigned int myID) {  }

    virtual unsigned int worker(unsigned int myID) {
        unsigned int i;
        for (i = 0; i < traces[myID]->size(); i++) {

            string k = traces[myID]->at(i);
            unsigned b = hash<string>{}(k) % Buckets;

            PersistentMap *map = pMaps[b];
            ObjectAlloc *alloc = map->getAllocator();

            locks[b].lock();

            // Create buffer for the value
            char *value = (char *)alloc->alloc(cfg->valueSize);
            memcpy(value, valueBuffer, cfg->valueSize);

            // Create buffer for the key
            char *key = (char *)alloc->alloc(k.length());
            strcpy(key, k.c_str());

            map->insert(key, value);
            locks[b].unlock();
        }
        return i;
    }

    void printReport() {
        cout << "persistent-hash_map,";
        Benchmark::printReport();
    }
};
#else
#include "../common/base.hpp"
#include "../../src/savitar.hpp"
#include <uuid/uuid.h>
#include <unordered_map>
#include <mutex>

using namespace std;
typedef char * T;

class PersistentHashMap : public PersistentObject {
public:
    typedef unordered_map<T, T, hash<T>, equal_to<T>, STLAlloc<T>> MapType;

    PersistentHashMap(uuid_t id) : PersistentObject(id) {
        hash<T> hasher;
        equal_to<T> cmp;
        for (unsigned b = 0; b < Buckets; b++) {
            void *t = alloc->alloc(sizeof(MapType));
            MapType *obj = (MapType *)t;
            vMaps[b] = new(obj) MapType(16, hasher, cmp, STLAlloc<T>(alloc));
        }
    }

    void insert(T key, T value) {
        // <compiler>
        Savitar_thread_notify(4, this, InsertTag, key, value);
        // </compiler>
        unsigned b = hash<string>{}(key) % Buckets;
        locks[b].lock();
        vMaps[b]->insert(make_pair(key, value));
        // <compiler>
        Savitar_thread_wait(this, this->log);
        // </compiler>
        locks[b].unlock();
    }

    // <compiler>
    PersistentHashMap() : PersistentObject(true) { }

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(PersistentHashMap));
        PersistentHashMap *obj = (PersistentHashMap *)temp;
        PersistentHashMap *object = new (obj) PersistentHashMap(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static PersistentHashMap *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        PersistentHashMap *obj =
            (PersistentHashMap *)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<PersistentHashMap *>(BaseFactory(id));
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

    static uint64_t classID() { return __COUNTER__; }
    // </compiler>

private:
    static const unsigned Buckets = 32;
    MapType *vMaps[Buckets];
    mutex locks[Buckets];
    // <compiler>
    enum MethodTags {
        InsertTag = 1,
    };
    // </compiler>
};

class NvHashMap : public Benchmark {
private:
    PersistentHashMap *pMap = NULL;

public:
    NvHashMap(Configuration *cfg) : Benchmark(cfg) {
        uuid_t uuid;
        uuid_generate(uuid);
        pMap = PersistentHashMap::Factory(uuid);
        assert(pMap != NULL);
    }

    virtual void init(unsigned int myID) {  }

    virtual void cleanup(unsigned int myID) {  }

    virtual unsigned int worker(unsigned int myID) {
        unsigned int i;
        ObjectAlloc *alloc = pMap->getAllocator();

        for (i = 0; i < traces[myID]->size(); i++) {

            string k = traces[myID]->at(i);

            // Create buffer for the value
            char *value = (char *)alloc->alloc(cfg->valueSize);
            memcpy(value, valueBuffer, cfg->valueSize);

            // Create buffer for the key
            char *key = (char *)alloc->alloc(k.length());
            strcpy(key, k.c_str());

            pMap->insert(key, value);
        }
        return i;
    }

    void printReport() {
        cout << "persistent-hash_map,";
        Benchmark::printReport();
    }
};

#endif // MULTI_OBJECT
