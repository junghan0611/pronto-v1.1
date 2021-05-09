#include <iostream>
#include <fstream>
#include <string>
#include <list>
#include <assert.h>
#include <thread>
#include <time.h>
#include "pmemkv.h"

#define MAX_KEY_SIZE    128

const string engine = "kvtree2";
struct Config {
    string tracePath;
    string dbPath;
    size_t dbSize;
    size_t valueSize;
    size_t threadCount;
};

using namespace std;
using namespace pmemkv;

list<string> keys;
size_t totalReady = 0;
bool waitForMain = true;

void worker(Config *config, size_t id) {
    // Open store
    string dbPath = config->dbPath + "." + to_string(id);
    KVEngine *kv = KVEngine::Open(engine, dbPath, config->dbSize);

    // Prepare value
    string value = "";
    for (size_t i = 0; i < config->valueSize - 1; i++) {
        value += (rand() % 2 == 0 ? 'a' : 'A') + (rand() % 26);
    }

    // Clone keys (make it thread-local)
    //list<string> myKeys = list<string>(keys);

    __sync_fetch_and_add(&totalReady, 1);
    while (waitForMain) { __sync_synchronize(); }

    // Run benchmark
    //for (string key : myKeys) {
    for (string key : keys) {
        KVStatus s = kv->Put(key, value);
        assert(s == OK);
    }
}

int main(int argc, char **argv) {

    // Default arguments
    Config config;
    config.tracePath = "";
    config.dbPath = "/mnt/ram/pmemkv";
    config.dbSize = (off_t)8 << 30;
    config.valueSize = 1024;
    config.threadCount = 1;

    if (argc <= 2) {
        cout << "Options:" << endl;
        cout << "- trace path" << endl;
        cout << "- database path" << endl;
        cout << "- database size (GB)" << endl;
        cout << "- value size" << endl;
        cout << "- operations count" << endl;
        cout << "- threads" << endl;
    }

    // Read arguments
    config.tracePath = argv[1];
    if (argc > 2) config.dbPath = argv[2];
    if (argc > 3) config.dbSize = (off_t)stoi(argv[3]) << 30;
    if (argc > 4) config.valueSize = stoi(argv[4]);
    if (argc > 5) config.threadCount = stoi(argv[5]);

    // Load keys from trace file
    ifstream trace(config.tracePath);
    char buffer[MAX_KEY_SIZE];

    size_t maxKeySize = 0;
    assert(trace.empty());
    while(trace) {
        trace.getline(buffer, MAX_KEY_SIZE);
        maxKeySize = maxKeySize > strlen(buffer) ? maxKeySize : strlen(buffer);
        if (trace) keys.push_back(buffer);
    }
    trace.close();

    // Create threads
    list<thread *> threads;
    for (size_t i = 0; i < config.threadCount; i++) {
        threads.push_back(new thread(worker, &config, i));
    }

    // Run benchmark
    struct timespec t1, t2;
    while (totalReady != config.threadCount) { __sync_synchronize(); }
    clock_gettime(CLOCK_REALTIME, &t1);
    __sync_bool_compare_and_swap(&waitForMain, true, false);

    // Wait for threads to complete
    for (auto t : threads) {
        t->join();
    }
    clock_gettime(CLOCK_REALTIME, &t2);

    // Compute output
    uint64_t total = (t2.tv_sec - t1.tv_sec) * 1E9;
    total += (t2.tv_nsec - t1.tv_nsec); // execution time (nanoseconds)
    double total_time = (double)total / 1E9;

    uint64_t total_ops = config.threadCount * keys.size();
    double throughput = total_ops / total_time; // throughput (ops/sec)

    double bw = (throughput * (config.valueSize + maxKeySize)) / 1024 / 1024; // bandwidth (MB/sec)

    // Print output
    fprintf(stdout, "%d,", (int)config.threadCount);
    fprintf(stdout, "%.2f,", total_time);
    fprintf(stdout, "%.2f,", throughput);
    fprintf(stdout, "%.2f\n", bw);

    // Cleanup
    threads.clear();
    keys.clear();

    return 0;
}
