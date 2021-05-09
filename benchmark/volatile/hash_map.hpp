#include "../common/base.hpp"
#include <unordered_map>
#include <mutex>

using namespace std;
typedef char * T;

class HashMap : public Benchmark {
    static const unsigned Buckets = 32;
    unordered_map<T, T> *vMaps[Buckets];
    mutex locks[Buckets];

public:
    HashMap(Configuration *cfg) : Benchmark(cfg) {
        for (unsigned b = 0; b < Buckets; b++) {
            vMaps[b] = new unordered_map<T, T>();
        }
    }

    ~HashMap() {
        for (unsigned b = 0; b < Buckets; b++) {
            unordered_map<T, T> *m = vMaps[b];
            for (auto it = m->begin(); it != m->end(); it++) {
                T key = it->first;
                T value = it->second;
                free(key);
                free(value);
            }
            delete m;
        }
    }

    virtual void init(unsigned int myID) {  }

    virtual void cleanup(unsigned int myID) {  }

    virtual unsigned int worker(unsigned int myID) {
        unsigned int i;
        for (i = 0; i < traces[myID]->size(); i++) {

            // Create buffer for the value
            char *value = (char *)malloc(cfg->valueSize);
            assert(value != NULL);
            memcpy(value, valueBuffer, cfg->valueSize);

            // Create buffer for the key
            char *key = (char *)malloc(traces[myID]->at(i).length());
            assert(key != NULL);
            strcpy(key, traces[myID]->at(i).c_str());

            unsigned b = hash<string>{}(key) % Buckets;
            locks[b].lock();
            vMaps[b]->insert(make_pair(key, value));
            locks[b].unlock();
        }
        return i;
    }

    void printReport() {
        cout << "hash_map,";
        Benchmark::printReport();
    }
};
