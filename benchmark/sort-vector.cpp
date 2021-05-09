#include <uuid/uuid.h>
#include "../src/savitar.hpp"
#include <vector>
#include <stack>
#include<signal.h>
#include <unistd.h>
#include <atomic>

#define OBJECT_UUID     "3b24574c-e920-4066-8eec-92f9e4682702"
#define DATA_SIZE       128 // 1 KB

using namespace std;
atomic<bool> stopThreads;

typedef struct DataElement {
    uint64_t data[DATA_SIZE];

    void operator = (const DataElement &b) {
        for (size_t i = 0; i < DATA_SIZE; i++) {
            data[i] = b.data[i];
        }
    }
} DataElement;

bool operator <= (const DataElement& lhs, const DataElement& rhs) {
    for (size_t i = 0; i < DATA_SIZE; i++) {
        if (lhs.data[i] > rhs.data[i]) return false;
    }
    return true;
}

class PersistentVector : public PersistentObject {
public:
    typedef DataElement T;
    PersistentVector(uuid_t id) : PersistentObject(id) {
        void *t = alloc->alloc(sizeof(vector<T, STLAlloc<T>>));
        vector<T, STLAlloc<T>> *obj = (vector<T, STLAlloc<T>>*)t;
        v_vector = new(obj) vector<T, STLAlloc<T>>((STLAlloc<T>(alloc)));
    }

    void push_back(T &value) {
        v_vector->push_back(value);
    }

    void swap(off_t a, off_t b) {
        // <compiler>
        Savitar_thread_notify(4, this, SwapTag, &a, &b);
        // </compiler>
        T t = v_vector->at(a);
        v_vector->at(a) = v_vector->at(b);
        v_vector->at(b) = t;
        // <compiler>
        Savitar_thread_wait(this, this->log);
        // </compiler>
    }

    size_t size() {
        return v_vector->size();
    }

    T &at(off_t offset) {
        return v_vector->at(offset);
    }

    // <compiler>
    PersistentVector() : PersistentObject(true) {}

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(PersistentVector));
        PersistentVector *obj = (PersistentVector*)temp;
        PersistentVector *object = new (obj) PersistentVector(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static PersistentVector *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        PersistentVector *obj = (PersistentVector*)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<PersistentVector*>(BaseFactory(id));
            manager.createNew(classID(), obj);
        }
        manager.unlock();
        return obj;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[3];

        switch (tag) {
            case SwapTag:
                {
                vector[0].addr = &tag;
                vector[0].len = sizeof(tag);
                vector[1].addr = (void*)args[0];
                vector[1].len = sizeof(uint64_t);
                vector[2].addr = (void*)args[1];
                vector[2].len = sizeof(uint64_t);
                vector_size = 3;
                }
                break;
            default:
                assert(false);
                break;
        }

        return AppendLog(vector, vector_size);
    }

    size_t Play(uint64_t tag, uint64_t *args, bool dry) {
        size_t bytes_processed = 0;
        switch (tag) {
            case SwapTag:
                {
                uint64_t a = args[0];
                uint64_t b = args[1];
                if (!dry) swap(a, b);
                bytes_processed += sizeof(a) + sizeof(b);
                }
                break;
            default:
                assert(false);
                break;
        }
        return bytes_processed;
    }

    static uint64_t classID() { return 1; }
    // </compiler>

private:
    vector<T, STLAlloc<T>> *v_vector;
    // <compiler>
    enum MethodTags {
        SwapTag = 1,
    };
    // </compiler>
};

uint64_t partition(PersistentVector *pv, uint64_t LB, uint64_t UB) {
    DataElement pivot = pv->at(UB);
    uint64_t i = LB - 1;
    for (uint64_t j = LB; j < UB; j++) {
        if (pv->at(j) <= pivot) {
            i++;
            pv->swap(i, j);
        }
    }
    pv->swap(i + 1, UB);
    return i + 1;
}

void quick_sort(PersistentVector *pv, uint64_t LB, uint64_t UB) {
    stack<pair<int64_t, int64_t>> callStack;
    callStack.push(pair<int64_t, int64_t>(LB, UB));

    while (!callStack.empty() && stopThreads == false) {
        int64_t lb = callStack.top().first;
        int64_t ub = callStack.top().second;
        callStack.pop();
        if (ub <= lb) continue;

        uint64_t pi = partition(pv, (uint64_t)lb, (uint64_t)ub);
        callStack.push(pair<int64_t, int64_t>(lb, pi - 1));
        callStack.push(pair<int64_t, int64_t>(pi + 1, ub));
    }
}

void *sort_thread(void *arg) {
    uint64_t *input = (uint64_t*)arg;
    uint64_t LB = input[0];
    uint64_t UB = input[1];
    PersistentVector *pv = (PersistentVector*)input[2];

    quick_sort(pv, LB, UB - 1);

    return NULL;
}

void randomFill(DataElement& t) {
    for (size_t i = 0; i < DATA_SIZE; i++) {
        t.data[i] = rand();
    }
}

int bench(int argc, char **argv) {

    uuid_t id;
    assert(uuid_parse(OBJECT_UUID, id) == 0);
    PersistentVector *pv = PersistentVector::Factory(id);

    if (pv->size() > 0) return 0; // do not continue after recovery

    assert(argc == 4);
    int snapshotFrequency = stoi(argv[2], nullptr); // sec
    assert(snapshotFrequency >= 2);
    size_t dataSize = (size_t)stoi(argv[1], nullptr) << 28; // GB
    int concurrencyLevel = stoi(argv[3], nullptr); // number of threads

    // 1. Fill the vector
    for (size_t i = 0; i < (dataSize / sizeof(DataElement)); i++) {
        DataElement t;
        randomFill(t);
        pv->push_back(t);
    }

    // 2. Run sort threads
    stopThreads = false;
    pthread_t *threads = (pthread_t*)calloc(concurrencyLevel, sizeof(pthread_t));
    assert(pv->size() % concurrencyLevel == 0);
    size_t partitionSize = pv->size() / concurrencyLevel;

    off_t LB = 0;
    for (int i = 0; i < concurrencyLevel; i++) {
        pthread_t thread;
        // range: [LB, UB)
        uint64_t *input = (uint64_t*)calloc(3, sizeof(uint64_t));
        input[0] = LB;
        LB += partitionSize;
        input[1] = LB;
        input[2] = (uint64_t)pv;
        Savitar_thread_create(&thread, NULL, sort_thread, input);
    }
    assert(LB == pv->size());

    // 3. Create periodic snapshots
    sleep(snapshotFrequency);
    raise(SIGUSR1);
    sleep(snapshotFrequency / 2);
    stopThreads = true;

    return 0;
}

int main(int argc, char **argv) {
    PersistentFactory::registerFactory<PersistentVector>();
    return Savitar_main(bench, argc, argv);
}
