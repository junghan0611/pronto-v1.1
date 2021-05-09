#include "../common/base.hpp"
#include <iostream>
#include <string.h>
#include <fstream>
#ifdef PRONTO
#include <rocksdb/pdb.h>
#else
#include <rocksdb/db.h>
#endif // PRONTO
#include <jemalloc/jemalloc.h>

#define MAX_KEY_SIZE        64
#define DB_PATH             "/mnt/ram/rocksdb"

using namespace std;
using namespace rocksdb;

class RocksDB : public Benchmark {
public:
    RocksDB(Configuration *cfg) : Benchmark(cfg) {
#ifdef PRONTO
        uuid_t uuid;
        uuid_generate(uuid);
        db = PDB::Factory(uuid);
#else
        Options options;
        options.create_if_missing = true;
        options.error_if_exists = true;
        options.compression = kNoCompression;
#ifdef ROCKSDB_ASYNC
        write_options.sync = false;
#else
        write_options.sync = true;
#endif // ROCKSDB_ASYNC
        write_options.disableWAL = false;
        Status status = DB::Open(options, DB_PATH, &db);
        if (!status.ok()) cout << status.getState() << endl;
        assert(status.ok());
#endif // PRONTO

        keys = (char **)malloc(sizeof(char *) * cfg->threads);
        values = (char **)malloc(sizeof(char *) * cfg->threads);

        // Populate database with the load data
        string workloadPath = cfg->workloadPrefix +
            "-load-" + to_string(cfg->threads) + ".";

        char *value = (char *)malloc(cfg->valueSize);
        memcpy(value, valueBuffer, cfg->valueSize);
        for (unsigned int thread = 0; thread < cfg->threads; thread++) {
            string op, key;
            ifstream infile(workloadPath + to_string(thread));
            while (infile >> op >> key) {
                db->Put(WriteOptions(), key, value);
            }
        }
        free(value);
#ifndef PRONTO
        db->FlushWAL(true);
#endif // PRONTO
    }

    ~RocksDB() {
        free(keys);
        free(values);
#ifndef PRONTO
        delete db;
#else
        db->Close();
#endif
    }

    // Nothing to do since RocksDB is MT-safe
    virtual void init(unsigned int myID) {
        keys[myID] = (char *)malloc(MAX_KEY_SIZE);
        values[myID] = (char *)malloc(cfg->valueSize);
        memcpy(values[myID], valueBuffer, cfg->valueSize);
    }

    virtual void cleanup(unsigned int myID) {
        free(keys[myID]);
        free(values[myID]);
    }

    virtual unsigned int worker(unsigned int myID) {
        Status status;
        unsigned int i = 0;
        char *key = keys[myID];
        char *value = values[myID];

        for (i = 0; i < traces[myID]->size(); i++) {
            string t = traces[myID]->at(i);
            string tag = t.substr(0, 3);
            if (tag == "Add" || tag == "Upd") {
                memcpy(value, valueBuffer, cfg->valueSize);
                if (tag == "Add")
                    strcpy(key, t.c_str() + 4);
                else // Update
                    strcpy(key, t.c_str() + 7);
                status = db->Put(write_options, key, value);
                if (status.ok()) i++;
            }
            else if (tag == "Rea") {
                string t_value;
                strcpy(key, t.c_str() + 5);
                db->Get(ReadOptions(), key, &t_value);
                if (status.ok()) i++;
            }
        }
        return i;
    }

    /*
    int totalKeys() {
        int totalKeys = 0;
        Iterator *it = db->NewIterator(ReadOptions());
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            if (it->value().ToString().length() == cfg->valueSize - 1) {
                totalKeys++;
            }
        }
        assert(it->status().ok());
        delete it;
        return totalKeys;
    }
    */

    void printReport() {
#ifdef PRONTO
        cout << "pronto";
#else
#ifdef ROCKSDB_ASYNC
        cout << "async";
#else
        cout << "sync";
#endif // ROCKSDB_ASYNC
#endif // PRONTO
        cout << "-rocksdb,";
        //cout << totalKeys() << ",";
        Benchmark::printReport();
    }
private:
#ifdef PRONTO
    PDB *db;
#else
    DB* db;
#endif // PRONTO
    WriteOptions write_options;
    char **keys;
    char **values;
};
