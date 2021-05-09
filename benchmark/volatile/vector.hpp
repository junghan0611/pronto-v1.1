#include "../common/base.hpp"
#include <vector>
#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

using namespace std;

class Vector : public Benchmark {
public:
    Vector(Configuration *cfg) : Benchmark(cfg) { }

    virtual void init(unsigned int myID) {
        vector<char *> *v = new vector<char *>();
        objects[myID] = (uintptr_t)v;
    }

    virtual void cleanup(unsigned int myID) {
        vector<char *> *v = (vector<char *> *)objects[myID];
        while (!v->empty()) {
            free(v->back());
            v->pop_back();
        }
        delete v;
    }

    virtual unsigned int worker(unsigned int myID) {
        vector<char *> *v = (vector<char *> *)objects[myID];
        unsigned int i = 0;
        for (i = 0; i < traces[myID]->size(); i++) {
            char *buffer = (char *)malloc(cfg->valueSize);
            assert(buffer != NULL);
            memcpy(buffer, valueBuffer, cfg->valueSize);
            v->push_back(buffer);
        }
        return i;
    }

    void printReport() {
        cout << "vector,";
        Benchmark::printReport();
    }
};
