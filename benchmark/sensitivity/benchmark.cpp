#include "../../src/savitar.hpp"
#include <uuid/uuid.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <unistd.h>

using namespace std;

inline uint64_t rdtsc() {
    unsigned int L, H;
    __asm__ __volatile__ ("rdtsc" : "=a" (L), "=d" (H));
    return ((uint64_t)H << 32) | L;
}

class BenchmarkObject : public PersistentObject {
public:
    BenchmarkObject(uuid_t id) : PersistentObject(id) {  }

    void meteredOp(char *data, uint64_t delay = 0) {
        // <compiler>
        Savitar_thread_notify(3, this, MeteredOpTag, data);
        // </compiler>

        uint64_t end = rdtsc() + delay;
        while(end > rdtsc()) {
            asm volatile (
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n" // 10
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n" // 20
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n" // 30
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n"
                    "nop\n" // 40
                    "lfence":::"memory");
        }

        // <compiler>
        Savitar_thread_wait(this, this->log);
        // </compiler>
    }

    // <compiler>
    BenchmarkObject() : PersistentObject(true) {}

    static PersistentObject *BaseFactory(uuid_t id) {
        ObjectAlloc *alloc = GlobalAlloc::getInstance()->newAllocator(id);
        void *temp = alloc->alloc(sizeof(BenchmarkObject));
        BenchmarkObject *obj = (BenchmarkObject *)temp;
        BenchmarkObject *object = new (obj) BenchmarkObject(id);
        return object;
    }

    static PersistentObject *RecoveryFactory(NVManager *m, CatalogEntry *e) {
        return BaseFactory(e->uuid);
    }

    static BenchmarkObject *Factory(uuid_t id) {
        NVManager &manager = NVManager::getInstance();
        manager.lock();
        BenchmarkObject *obj = (BenchmarkObject *)manager.findRecovered(id);
        if (obj == NULL) {
            obj = static_cast<BenchmarkObject *>(BaseFactory(id));
            manager.createNew(classID(), obj);
        }
        manager.unlock();
        return obj;
    }

    uint64_t Log(uint64_t tag, uint64_t *args) {
        int vector_size = 0;
        ArgVector vector[2]; // Max arguments of the class

        switch (tag) {
            case MeteredOpTag:
                {
                vector[0].addr = &tag;
                vector[0].len = sizeof(tag);
                vector[1].addr = (void *)args[0];
                vector[1].len = strlen((char *)args[0]) + 1;
                vector_size = 2;
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
            case MeteredOpTag:
                {
                char *data = (char *)args;
                if (!dry) meteredOp(data);
                bytes_processed = strlen(data) + 1;
                }
                break;
            default:
                {
                PRINT("Unknown tag: %zu\n", tag);
                assert(false);
                }
                break;
        }
        return bytes_processed;
    }

    static uint64_t classID() { return 1; }
    // </compiler>
private:
    // <compiler>
    enum MethodTags {
        MeteredOpTag = 1,
    };
    // </compiler>
};

void randomMemset(char *ptr, size_t size) {
    for (size_t i = 0; i < size; i++) {
        char c = rand() % 2 == 0 ? 'A' : 'a';
        c += (rand() % 26);
        ptr[i] = c;
    }
}

uint64_t nanosecondsToCycles(uint64_t delay) {
    struct timespec t1, t2;
    uint64_t cycles[2];

    clock_gettime(CLOCK_REALTIME, &t1);
    cycles[0] = rdtsc();
    clock_gettime(CLOCK_REALTIME, &t1);
    usleep(250000);
    clock_gettime(CLOCK_REALTIME, &t2);
    cycles[1] = rdtsc();
    clock_gettime(CLOCK_REALTIME, &t2);

    uint64_t nanoseconds = (t2.tv_sec - t1.tv_sec) * 1E9;
    nanoseconds += t2.tv_nsec - t1.tv_nsec;
    return (delay * (cycles[1] - cycles[0])) / nanoseconds;
}

int benchmark_main(int argc, char **argv) {
    if (argc < 3) {
        cout << "./benchmark operation-latency value-size" << endl;
        return 1;
    }

    const size_t opCount = 1E6;
    unsigned opLatency = stoi(argv[1], nullptr, 10);
    size_t valueSize = stoi(argv[2], nullptr, 10);
    struct timespec t1, t2;

    uuid_t uuid;
    uuid_generate(uuid);
    BenchmarkObject *object = BenchmarkObject::Factory(uuid);

    char *value = (char *)aligned_alloc(64, valueSize);
    randomMemset(value, valueSize - 1);
    value[valueSize - 1] = '\0';

    clock_t delay = nanosecondsToCycles(opLatency);

    clock_gettime(CLOCK_REALTIME, &t1);
    for (size_t op = 0; op < opCount; op++) {
        object->meteredOp(value, delay);
    }
    clock_gettime(CLOCK_REALTIME, &t2);

    uint64_t execTime = (t2.tv_sec - t1.tv_sec) * 1E9;
    execTime += t2.tv_nsec - t1.tv_nsec;

    fprintf(stdout, "%zu,%u,", valueSize, opLatency);
    fprintf(stdout, "%zu,", execTime);
    fprintf(stdout, "%.*f\n", 2, (float)execTime / opCount);

    free(value);
    return 0;
}

int main(int argc, char **argv) {
    PersistentFactory::registerFactory<BenchmarkObject>();
    return Savitar_main(benchmark_main, argc, argv);
}
