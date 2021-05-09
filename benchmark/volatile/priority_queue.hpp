#include "../common/base.hpp"
#include <queue>
#include <iostream>
#include <string.h>

using namespace std;

class PriorityQueue : public Benchmark {
public:
    PriorityQueue(Configuration *cfg) : Benchmark(cfg) { }

    virtual void init(unsigned int myID) {
        priority_queue<char *> *q = new priority_queue<char *>();
        objects[myID] = (uintptr_t)q;
    }

    virtual void cleanup(unsigned int myID) {
        priority_queue<char *> *q = (priority_queue<char *> *)objects[myID];
        while (!q->empty()) {
            char *ptr = static_cast<char *>(q->top());
            q->pop();
            free(ptr);
        }
        delete q;
    }

    virtual unsigned int worker(unsigned int myID) {
        priority_queue<char *> *q = (priority_queue<char *> *)objects[myID];
        unsigned int i = 0;
        for (i = 0; i < traces[myID]->size(); i++) {
            char *buffer = (char *)malloc(cfg->valueSize);
            assert(buffer != NULL);
            memcpy(buffer, valueBuffer, cfg->valueSize);
            q->push(buffer);
        }
        return i;
    }

    void printReport() {
        cout << "priority_queue,";
        Benchmark::printReport();
    }
};
