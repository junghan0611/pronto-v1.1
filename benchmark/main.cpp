#include <string>
#include "../src/savitar.hpp"
#include "volatile/vector.hpp"
#include "volatile/priority_queue.hpp"
#include "volatile/unordered_map.hpp"
#include "volatile/map.hpp"
#include "volatile/hash_map.hpp"
#ifdef ROCKSDB
#include "rocksdb/rocksdb.hpp"
#endif
#include "persistent/vector.hpp"
#include "persistent/priority_queue.hpp"
#include "persistent/unordered_map.hpp"
#include "persistent/map.hpp"
#include "persistent/hash_map.hpp"
#include "persistent/pmemkv.hpp"
#include "recovery/breakdown.hpp"
#include "recovery/overhead.hpp"

using namespace std;

void createConfiguration(int argc, char **argv, Configuration &cfg) {
    cfg.threads = 1;
    if (argc > 3) cfg.threads = stoi(argv[3]);
    cfg.valueSize = 1024;
    if (argc > 4) cfg.valueSize = stoi(argv[4]);
    cfg.workloadPrefix = argv[2];
}

template <class T>
int runBenchmark(int argc, char **argv, bool useSavitar) {
    Configuration cfg;
    createConfiguration(argc, argv, cfg);
    cfg.useSavitar = useSavitar;

    T bench(&cfg);
    bench.run();
    bench.printReport();

    return 0;
}

template <class T>
int runBenchmark(int argc, char **argv) {
    return runBenchmark<T>(argc, argv, false);
}

template <class T>
int runNvBenchmark(int argc, char **argv) {
    return runBenchmark<T>(argc, argv, true);
}

int runRecoveryBreakdown(int argc, char **argv) {
    size_t numberOfModifiedPages = stoi(argv[2]);
    RecoveryBreakdown bench(numberOfModifiedPages);
    bench.run();
    cout << "recovery-breakdown,";
    cout << bench.getSynchronousLatency() << ",";
    cout << bench.getAsynchronousLatency() << endl;
    return 0;
}

int runRecoveryOverhead(int argc, char **argv) {
    size_t numberOfPages = stoi(argv[2]);
    RecoveryOverhead bench;
    bench.setNumberOfPages(numberOfPages);

    bool randomAccess = false;
    if (argc > 3) randomAccess = stoi(argv[3]) > 0;
    bench.setRandomAccess(randomAccess);

    unsigned writeRatio = 50;
    if (argc > 4) writeRatio = stoi(argv[4]);
    bench.setWriteRatio(writeRatio);

    size_t operationCount = 5000000;
    if (argc > 5) operationCount = stoi(argv[5]);
    bench.setOperationCount(operationCount);

    bench.run();

    cout << "recovery-overhead,";
    cout << bench.getExecTimeWithNoSnapshot() << ",";
    cout << bench.getExecTimeWithSnapshots() << endl;

    return 0;
}

int main(int argc, char **argv) {

    if (argc < 3) {
        cout << "./benchmark benchmark workload-prefix";
        cout << " | recovery-configurations ";
        cout << "[threads] [value-size]" << endl;
        return 1;
    }

    if (strcmp(argv[1], "volatile-vector") == 0)
        return runBenchmark<Vector>(argc, argv);
    else if (strcmp(argv[1], "volatile-queue") == 0)
        return runBenchmark<PriorityQueue>(argc, argv);
    else if (strcmp(argv[1], "volatile-map") == 0)
        return runBenchmark<Map>(argc, argv);
    else if (strcmp(argv[1], "volatile-ordered-map") == 0)
        return runBenchmark<OrderedMap>(argc, argv);
    else if (strcmp(argv[1], "volatile-hash-map") == 0)
        return runBenchmark<HashMap>(argc, argv);
#ifdef ROCKSDB
    else if (strcmp(argv[1], "rocksdb") == 0) {
#ifdef PRONTO
        PersistentFactory::registerFactory<PDB>();
        return Savitar_main(runNvBenchmark<RocksDB>, argc, argv);
#else
        return runBenchmark<RocksDB>(argc, argv);
#endif // PRONTO
    }
#endif // ROCKSDB
    else if (strcmp(argv[1], "persistent-vector") == 0) {
        PersistentFactory::registerFactory<PersistentVector>();
        return Savitar_main(runNvBenchmark<NvVector>, argc, argv);
    }
    else if (strcmp(argv[1], "persistent-queue") == 0) {
        PersistentFactory::registerFactory<PersistentPriorityQueue>();
        return Savitar_main(runNvBenchmark<NvPriorityQueue>, argc, argv);
    }
    else if (strcmp(argv[1], "persistent-map") == 0) {
        PersistentFactory::registerFactory<PersistentMap>();
        return Savitar_main(runNvBenchmark<NvMap>, argc, argv);
    }
    else if (strcmp(argv[1], "persistent-ordered-map") == 0) {
        PersistentFactory::registerFactory<PersistentOrderedMap>();
        return Savitar_main(runNvBenchmark<NvOrderedMap>, argc, argv);
    }
    else if (strcmp(argv[1], "persistent-hash-map") == 0) {
#ifdef MULTI_OBJECT
        PersistentFactory::registerFactory<PersistentMap>();
#else
        PersistentFactory::registerFactory<PersistentHashMap>();
#endif
        return Savitar_main(runNvBenchmark<NvHashMap>, argc, argv);
    }
    else if (strcmp(argv[1], "pmemkv") == 0)
        return runBenchmark<PMEMKV>(argc, argv);
    else if (strcmp(argv[1], "recovery-breakdown") == 0) {
        return runRecoveryBreakdown(argc, argv);
    }
    else if (strcmp(argv[1], "recovery-overhead") == 0) {
        return runRecoveryOverhead(argc, argv);
    }

    return 1;
}
