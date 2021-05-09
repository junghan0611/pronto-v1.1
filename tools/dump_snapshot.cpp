#include <assert.h>
#include <uuid/uuid.h>
#include <iostream>
#include <fcntl.h>
#include <sys/mman.h>
#include <ctime>
#include <iostream>
#include <string.h>
#include "../src/snapshot.hpp"
#include "../src/ckpt_alloc.hpp"

using namespace std;

snapshot_header_t *loadSnapshot(const char *path) {
    snapshot_header_t *snapshot = NULL;
    int fd = open(path, O_RDONLY, 0666);
    assert(fd > 0);
    snapshot = (snapshot_header_t *)mmap(NULL, sizeof(snapshot_header_t),
            PROT_READ, MAP_SHARED, fd, 0);
    assert(snapshot != NULL);
    size_t size = snapshot->size;
    munmap(snapshot, sizeof(snapshot_header_t));
    snapshot = (snapshot_header_t *)mmap(NULL, size,
            PROT_READ, MAP_SHARED, fd, 0);
    assert(snapshot != NULL);
    close(fd);
    return snapshot;
}

int main(int argc, char **argv) {

    assert(argc == 2);
    const char *path = argv[1];
    snapshot_header_t *header = loadSnapshot(path);

    // Snapshot header
    time_t time = (time_t)header->time;
    tm *ptm = localtime(&time);
    char buffer[32];
    std::strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", ptm);
    cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=";
    cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    cout << "ID:\t\t\t" << header->identifier << endl;
    cout << "Time:\t\t\t" << buffer << endl;
    cout << "Size:\t\t\t" << (header->size >> 20) << " MB" << endl;
    cout << "Synchronous:\t\t" << (header->sync_latency / 1E3) << " ms" << endl;
    cout << "Asynchronous:\t\t" << (header->async_latency / 1E3) << " ms" << endl;

    cout << "Objects:\t\tIdentifier | Object pointer | Last commit ID" << endl;
    char *ptr = (char *)header + header->alloc_offset;
    for (unsigned int i = 0; i < header->object_count; i++) {
        uint64_t commit = *((uint64_t *)ptr);
        ptr = ptr + sizeof(uint64_t);
        uint64_t logTail = *((uint64_t *)ptr);
        ptr = ptr + sizeof(uint64_t);
        uintptr_t obj = *((uintptr_t *)ptr);
        ptr = ptr + sizeof(uintptr_t);
        uuid_t uuid;
        memcpy(uuid, ptr, sizeof(uuid_t));
        char uuid_str[64];
        uuid_unparse(uuid, uuid_str);
        printf("[%d]\t\t\t%s | %p | %zu\n", i, uuid_str, (void *)obj, commit);

        uint64_t allocTotalCores = ((uint64_t *)ptr)[2];
        size_t allocSnapshotSize = sizeof(uuid_t);
        allocSnapshotSize += sizeof(uint64_t) + sizeof(uint64_t);
        allocSnapshotSize += FreeList::snapshotSize() * allocTotalCores;
        ptr += allocSnapshotSize;
    }

    size_t allocated = *((uint64_t *)((char *)header + header->global_offset));
    size_t allocatedPages = allocated / GlobalAlloc::BitmapGranularity;

    uint64_t *bitmap = (uint64_t *)((char *)header + header->bitmap_offset);
    size_t modifiedPages = 0, totalPages = 0;
    size_t bitmapSize =
        (GlobalAlloc::MaxMemorySize / GlobalAlloc::BitmapGranularity) >> 3;
    for (size_t i = 0; i < bitmapSize; i++) {
        uint64_t bit = bitmap[i];
        for (size_t p = 0; p < 64; p++) {
            if (bit & 0x0000000000000001) modifiedPages++;
            totalPages++;
            bit >> 1;
        }
        if (totalPages == allocatedPages) break;
    }
    cout << "Modified pages:\t\t" << modifiedPages << " (" <<
        (modifiedPages * GlobalAlloc::BitmapGranularity >> 10) << " KB)"<< endl;
    cout << "Total allocated:\t" << (allocated >> 20) << " MB" << endl;
    cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=";
    cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;

    return 0;
}
