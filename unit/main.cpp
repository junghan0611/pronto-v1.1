#include "gtest/gtest.h"
#include <limits.h>
#include <stdint.h>
#include "alloc_bitmap.hpp"
#include "alloc_global.hpp"
#include "alloc_object.hpp"
#include "alloc_free_list.hpp"
#include "snapshot.hpp"
#include "../src/savitar.hpp"

namespace {
    int main(int argc, char **argv) {
        testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
}
