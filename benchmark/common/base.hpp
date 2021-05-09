#ifndef BENCH_HEADER
#define BENCH_HEADER
#include <vector>
#include <stdint.h>
#include <assert.h>
#define JEMALLOC_MANGLE
#include <jemalloc/jemalloc.h>
#include <string>

using namespace std;

class Configuration {
public:
    unsigned int threads;
    unsigned int operations;
    size_t valueSize;
    string workloadPrefix;
    bool useSavitar;
};

class Benchmark {
public:
    Benchmark(Configuration *);
    ~Benchmark();
    static Benchmark *singleton() {
        return instance;
    }

    virtual void init(unsigned int) = 0;
    virtual void cleanup(unsigned int) = 0;
    virtual unsigned int worker(unsigned int) = 0;

    void run();
    static void *worker(void *);
    uint64_t getLatency();
    uint64_t getThroughput();
    virtual void printReport();

protected:
    static Benchmark *instance;
    Configuration *cfg;
    struct timespec t1, t2;
    uint64_t sync;
    char *valueBuffer;
    vector<string> **traces;
    uintptr_t *objects;
};

#endif
