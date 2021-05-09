#include <assert.h>
#include <uuid/uuid.h>
#include <iostream>
#include "../src/nv_log.hpp"
#include "../src/savitar.hpp"
#define NESTED_TX_TAG               0x8000000000000000

using namespace std;

int main(int argc, char **argv) {
    uuid_t uuid;
    assert(argc == 2);
    assert(uuid_parse(argv[1], uuid) == 0);

    SavitarLog *log = Savitar_log_open(uuid);
    assert(log != NULL);

    size_t log_size = log->size / 1024 / 1024; // MB

    cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=";
    cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    cout << "UUID:\t\t" << argv[1] << endl;
    cout << "Size:\t\t" << log_size << " MB" << endl;
    cout << "Head:\t\t" << log->head << endl;
    cout << "Tail:\t\t" << log->tail;
    if (log->tail > log->size) {
        cout << " (exceeded capacity by ";
        cout << (log->tail - log->size);
        cout << " bytes)";
    }
    cout << endl;
    cout << "Last commit:\t" << log->last_commit << endl;
    cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=";
    cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    cout << "Offset\tMagic\t\t\tCommit\tTag\tParent object UUID\t\t\tOffset" << endl;

    off_t offset = sizeof(SavitarLog);
    char *data = (char *)log;

    while (offset < log->tail && offset < log->size) {
        uint64_t magic = *reinterpret_cast<uint64_t *>(&data[offset + 8]);
        if (magic != REDO_LOG_MAGIC) { offset+= 64; continue; }
        cout << "[" << offset << "]\t";
        cout << std::hex << magic << "\t";
        cout << std::dec << *((uint64_t *)(&data[offset])) << "\t";
        uint64_t method_tag = *((uint64_t *)&data[offset + 16]);
        if (method_tag & NESTED_TX_TAG) {
            cout << "-\t";
            struct uuid_wrapper {
                uuid_t uuid;
            } *uuid_ptr = (struct uuid_wrapper *)&data[offset + 24];
            char uuid_str[64];
            uuid_unparse(uuid_ptr->uuid, uuid_str);
            cout << uuid_str << "\t" << (method_tag & (~NESTED_TX_TAG));
        }
        else {
            cout << method_tag << "\t-\t\t\t\t\t-";
        }

        cout << endl;
        offset += 64;
    }

    cout << "=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=";
    cout << "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=" << endl;
    Savitar_log_close(log);

    return 0;
}
