#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>



int main(int argc, char **argv) {
    VMinitialize();
    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        printf("writing to %llu\n", (long long int) i);
        VMwrite(5 * i * PAGE_SIZE, i);
    }

    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
        word_t value;
        VMread(5 * i * PAGE_SIZE, &value);
        printf("reading from %llu %d\n", (long long int) i, value);
        assert(uint64_t(value) == i);
    }
    printf("success\n");

    return 0;

    //PDF example:
//    VMwrite(13,3);
//    word_t value1;
//    VMread(13, &value1);
//    printf("%d\n", value1);
//
//    VMwrite(6,7);
//    word_t value2;
//    VMread(6, &value2);
//    printf("%d\n", value2);
//
//    VMwrite(31,1);
//    word_t value3;
//    VMread(31, &value3);
//    printf("%d\n", value3);
}
