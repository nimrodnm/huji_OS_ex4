#include "VirtualMemory.h"

#include <cstdio>
#include <cassert>
#include <inttypes.h>
#include <cmath>

#define PAGE_ADDRESS_WIDTH static_cast<uint64_t> (log2(PAGE_SIZE))

uint64_t GetPi(uint64_t virtualAddress, int index) {
    // creates a mask with log2(PAGE_SIZE) bits turned on, on the right:
//    // shifts the mask to position of p_index:
//    mask = mask << (VIRTUAL_ADDRESS_WIDTH - (PAGE_ADDRESS_WIDTH * index));
//    return virtualAddress & mask;

    uint64_t mask = PAGE_SIZE - 1;
    return (virtualAddress >> (VIRTUAL_ADDRESS_WIDTH - (PAGE_ADDRESS_WIDTH * index))) & mask;
}

int main(int argc, char **argv) {
//    VMinitialize();
//    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
//        printf("writing to %llu\n", (long long int) i);
//        VMwrite(5 * i * PAGE_SIZE, i);
//    }
//
//    for (uint64_t i = 0; i < (2 * NUM_FRAMES); ++i) {
//        word_t value;
//        VMread(5 * i * PAGE_SIZE, &value);
//        printf("reading from %llu %d\n", (long long int) i, value);
//        assert(uint64_t(value) == i);
//    }
//    printf("success\n");
//
//    return 0;
    printf("The uint64_t value is: %" PRIu64 "\n", GetPi(62775, 1));
    printf("The uint64_t value is: %" PRIu64 "\n", GetPi(62775, 2));
    printf("The uint64_t value is: %" PRIu64 "\n", GetPi(62775, 3));
    printf("The uint64_t value is: %" PRIu64 "\n", GetPi(62775, 4));
    printf("The uint64_t value is: %" PRIu64 "\n", GetPi(62775, 5));
}
