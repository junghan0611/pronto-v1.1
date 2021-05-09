#pragma once
#include <uuid/uuid.h>
#include <savitar.hpp>
#include "rocksdb/db.h"
#include <signal.h>

namespace rocksdb {

class PDB : public PersistentObject {
public:
    PDB(uuid_t id) :
        PersistentObject(id) {

        // Configure DB
        Options options(alloc);
        string db_path = string(PMEM_PATH) + "rocksdb";
        options.create_if_missing = true;
        options.error_if_exists = true;
        options.compression = kNoCompression;
        options.manual_wal_flush = true;
        options.statistics = rocksdb::CreateDBStatistics();
        //options.max_background_jobs = 0;
        options.write_buffer_size = (off_t)4 << 30;

        pWriteOptions.disableWAL = true;
        pWriteOptions.sync = false;

        DB::Open(options, db_path, &db);
    }

    void Close() { raise(SIGUSR1); }

    Status Get(const ReadOptions &options, const Slice& key,
            string* value) {
        return db->Get(options, key, value);
    }

    Status Put(const WriteOptions &options, const Slice& key,
            const Slice& value) {
        // <compiler>
        Savitar_thread_notify(4, this, PutDefaultColumnFamily,
                &key, &value);
        // </compiler>
        Status st = db->Put(pWriteOptions, key, value);
        // DEBUG
        Savitar_thread_wait(this, this->log);
        // DEBUG
        return st;
    }

    // <compiler>
    PDB() : PersistentObject(true) { }

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(PDB));
        PDB *obj = (PDB *)temp;
        PDB *object = new (obj) PDB(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static PDB *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        PDB *obj = (PDB *)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<PDB *>(BaseFactory(id));
            manager.createNew(classID(), obj);
        }
        manager.unlock();
        return obj;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[3]; // Max arguments of the class

        switch (tag) {
            case PutDefaultColumnFamily:
                {
                vector[0].addr = &tag;
                vector[0].len = sizeof(tag);
                // Slice : key
                Slice *key = (Slice *)args[0];
                vector[1].addr = (void *)key->data();
                vector[1].len = key->size();
                // Slice : value
                Slice *value = (Slice *)args[1];
                vector[2].addr = (void *)value->data();
                vector[2].len = value->size();
                vector_size = 3;
                }
                break;
            case PutColumnFamilyHandle:
            case DeleteDefaultColumnFamily:
            case DeleteColumnFamilyHandle:
            case SingleDeleteDefaultColumnFamily:
            case SingleDeleteColumnFamilyHandle:
            case DeleteRangeColumnFamilyHandle:
            case MergeDefaultColumnFamily:
            case MergeColumnFamilyHandle:
            case WriteBatch:
                assert(false); // Not supported
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
            case PutDefaultColumnFamily:
                {
                char *ptr = (char *)args;
                const char *key = ptr;
                const char *value = ptr + strlen(key);
                if (!dry) db->Put(pWriteOptions, key, value);
                bytes_processed = strlen(key) + strlen(value) + 2;
                }
                break;
            case PutColumnFamilyHandle:
            case DeleteDefaultColumnFamily:
            case DeleteColumnFamilyHandle:
            case SingleDeleteDefaultColumnFamily:
            case SingleDeleteColumnFamilyHandle:
            case DeleteRangeColumnFamilyHandle:
            case MergeDefaultColumnFamily:
            case MergeColumnFamilyHandle:
            case WriteBatch:
                {
                PRINT("Tag not supported: %zu\n", tag);
                assert(false); // Not supported
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
    DB *db = NULL;
    WriteOptions pWriteOptions;

    // <compiler>
    enum MethodTags {
        PutDefaultColumnFamily = 1,
        PutColumnFamilyHandle = 2,
        DeleteDefaultColumnFamily = 3,
        DeleteColumnFamilyHandle = 4,
        SingleDeleteDefaultColumnFamily = 5,
        SingleDeleteColumnFamilyHandle = 6,
        DeleteRangeColumnFamilyHandle = 7,
        MergeDefaultColumnFamily = 8,
        MergeColumnFamilyHandle = 9,
        WriteBatch = 10,
        // TODO support for other methods
    };
    // </compiler>
};

} // namespace rocksdb
