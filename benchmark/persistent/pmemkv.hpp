#include "../common/base.hpp"
#include <pmemkv.h>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;
using namespace pmemkv;

class PMEMKV : public Benchmark {
public:
    PMEMKV(Configuration *cfg) : Benchmark(cfg) {  }

    virtual void init(unsigned int myID) {
        // TOOD get pool path from a global variable
        string path = "/mnt/ram/pmemkv." + to_string(myID);
        size_t size = ((size_t)24 << 30);
        size /= cfg->threads;
        KVEngine *kv = KVEngine::Open("kvtree2", path, size);
        objects[myID] = (uintptr_t)kv;
    }

    virtual void cleanup(unsigned int myID) {
        KVEngine *kv = (KVEngine *)objects[myID];
        KVEngine::Close(kv);
    }

    virtual unsigned int worker(unsigned int myID) {
        KVEngine *kv = (KVEngine *)objects[myID];
        unsigned int i = 0;
        for (i = 0; i < traces[myID]->size(); i++) {
            // Create buffer for the value
            char *value = (char *)malloc(cfg->valueSize);
            assert(value != NULL);
            memcpy(value, valueBuffer, cfg->valueSize);

            // Create buffer for the key
            char *key = (char *)malloc(traces[myID]->at(i).length());
            assert(key != NULL);
            strcpy(key, traces[myID]->at(i).c_str());

            KVStatus s = kv->Put(key, value);
            assert(s == OK);
        }
        return i;
    }

    void printReport() {
        cout << "pmemkv,";
        Benchmark::printReport();
    }
};
