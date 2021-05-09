#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include "base.hpp"
#include "savitar.hpp"

using namespace std;

Benchmark *Benchmark::instance = NULL;

Benchmark::Benchmark(Configuration *config) {
    assert(instance == NULL);
    instance = this;
    cfg = config;
    memset(&t1, 0, sizeof(t1));
    memset(&t2, 0, sizeof(t2));
    sync = 0;

    // Populate value
    valueBuffer = (char *)malloc(cfg->valueSize);
    for (size_t i = 0; i < cfg->valueSize - 1; i++) {
        valueBuffer[i] = (rand() % 2 == 0 ? 'A' : 'a') + (i % 26);
    }
    valueBuffer[cfg->valueSize - 1] = 0;

    // Populate traces
    config->operations = 0;
    traces =
        (vector<string> **)malloc(sizeof(vector<string> *) * cfg->threads);
    for (unsigned int thread = 0; thread < cfg->threads; thread++) {
        traces[thread] = new vector<string>();
        string workloadPath = cfg->workloadPrefix;
#ifdef ROCKSDB
        workloadPath += "-run-" + to_string(cfg->threads) + ".";
#endif
        workloadPath += to_string(thread);

        string temp;
        ifstream infile(workloadPath);
        while (infile >> temp) {
            traces[thread]->push_back(temp);
            config->operations++;
        }
    }

    objects = (uintptr_t *)malloc(sizeof(uintptr_t) * cfg->threads);
}

Benchmark::~Benchmark() {
    free(valueBuffer);
    for (unsigned int thread = 0; thread < cfg->threads; thread++) {
        traces[thread]->clear();
        delete traces[thread];
    }
    free(traces);
    free(objects);
    instance = NULL;
}

void Benchmark::run() {

    unsigned int threads = cfg->threads;
    pthread_t *t = (pthread_t *)malloc(sizeof(pthread_t) * threads);
    unsigned int *threadIDs =
        (unsigned int *)malloc(sizeof(unsigned int) * threads);

    for (unsigned int i = 0; i < threads - 1; i++) {
        threadIDs[i] = i;
        if (cfg->useSavitar)
            Savitar_thread_create(&t[i], NULL, worker, &threadIDs[i]);
        else
            pthread_create(&t[i], NULL, worker, &threadIDs[i]);
    }

    threadIDs[threads - 1] = threads - 1;
    void *retVal = worker(&threadIDs[threads - 1]);
    unsigned int ops = *((unsigned int *)retVal);
    free(retVal);

    for (unsigned i = 0; i < threads - 1; i++) {
        pthread_join(t[i], &retVal);
        ops += *((unsigned int *)retVal);
        free(retVal);
    }

    assert(ops == cfg->operations);
    free(t);
    free(threadIDs);
}

void *Benchmark::worker(void *arg) {

    Benchmark *me = Benchmark::singleton();
    unsigned int myID = *((unsigned int *)arg);

    me->init(myID);
    uint64_t t = __sync_add_and_fetch(&me->sync, 1);
    if (t == me->cfg->threads) clock_gettime(CLOCK_REALTIME, &me->t1);
    while (me->sync != me->cfg->threads) { __sync_synchronize(); }

    unsigned int ops = me->worker(myID);

    t = __sync_sub_and_fetch(&me->sync, 1);
    if (t == 0) clock_gettime(CLOCK_REALTIME, &me->t2);
    me->cleanup(myID);

    unsigned int *ret = (unsigned int *)malloc(sizeof(unsigned int));
    *ret = ops;
    return (void *)ret;
}

uint64_t Benchmark::getLatency() {
    uint64_t total = (t2.tv_sec - t1.tv_sec) * 1E9;
    total += (t2.tv_nsec - t1.tv_nsec);
    return total;
}

uint64_t Benchmark::getThroughput() {
    uint64_t totalOps = cfg->operations;
    uint64_t execTime = getLatency();
    return execTime > cfg->operations ? (totalOps * 1E9) / execTime : 0;
}

void Benchmark::printReport() {
    cout << (getLatency() / cfg->operations) << "," << getThroughput() << endl;
}
