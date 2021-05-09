#include "../common/base.hpp"
#include <map>
#include <iostream>
#include <string.h>

using namespace std;
typedef char * T;

class OrderedMap : public Benchmark {
public:
    OrderedMap(Configuration *cfg) : Benchmark(cfg) { }

    virtual void init(unsigned int myID) {
        map<T, T> *m = new map<T, T>();
        objects[myID] = (uintptr_t)m;
    }

    virtual void cleanup(unsigned int myID) {
        map<T, T> *m = (map<T, T> *)objects[myID];
        for (auto it = m->begin(); it != m->end(); it++) {
            char *key = it->first;
            char *value = it->second;
            free(key);
            free(value);
        }
        delete m;
    }

    virtual unsigned int worker(unsigned int myID) {
        map<T, T> *m = (map<T, T> *)objects[myID];
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

            m->insert(make_pair(key, value));
        }
        return i;
    }

    void printReport() {
        cout << "ordered-map,";
        Benchmark::printReport();
    }
};
