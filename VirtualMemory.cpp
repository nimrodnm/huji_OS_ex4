#pragma once

#include <cmath>

#include "VirtualMemory.h"
#include "PhysicalMemory.h"

// TODO: is PAGE_SIZE a power of 2?
#define PAGE_ADDRESS_WIDTH static_cast<uint64_t> (log2(PAGE_SIZE))

typedef struct {
    bool foundEmpty;
    word_t maxFrameIndex;
    uint64_t maxCyclicDist;
    word_t resultFrameIdx;
    word_t resultFrameParentIdx;
    uint64_t resultFrameOffsetInParent;
    uint64_t resultPageIdx;
} DfsData;


uint64_t getPi(uint64_t virtualAddress, int index) {
    // creates a mask with log2(PAGE_SIZE) bits turned on, on the right:
    uint64_t mask = PAGE_SIZE - 1;
    // shifts the virtualAddress to the right according to the index given and activates the mask:
    return (virtualAddress >> (VIRTUAL_ADDRESS_WIDTH - (PAGE_ADDRESS_WIDTH * index))) & mask;
}

uint64_t getOffset(uint64_t virtualAddress) {
    // creates a mask with OFFSET_WIDTH bits turned on, on the right:
    uint64_t mask = (1LL << OFFSET_WIDTH) - 1;
    // activates the mask:
    return virtualAddress & mask;
}

uint64_t getPageIdx(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}


//TODO: handle PMread/write/etc errors.

DfsData handlePageFaultHelper(uint64_t virtualAddress,
                              uint64_t cumulativePageIdx,
                              int currDepth,
                              word_t currFrameIdx,
                              word_t ignoreFrameIdx) {
    DfsData dfsData = {false, currFrameIdx, 0, 0, 0, 0, cumulativePageIdx};
    //TODO: TABLES_DEPTH or TABLES_DEPTH-1?
    if (currDepth == TABLES_DEPTH) {
        dfsData.maxCyclicDist = calculateCyclicDistance(cumulativePageIdx, getPageIdx(virtualAddress));
        dfsData.resultFrameIdx = currFrameIdx;
        return dfsData;
    }

    bool emptyFrame = true;

    for (uint64_t offset = 0; offset < PAGE_SIZE; offset++) {
        word_t currChildFrameIdx;
        PMread((currFrameIdx * PAGE_SIZE) + offset, &currChildFrameIdx);
        if (currChildFrameIdx != 0) {
            emptyFrame = false;
            DfsData currChildDfsData = handlePageFaultHelper(virtualAddress,
                                                          updateCumulativePageIdx(cumulativePageIdx, offset),
                                                          currDepth + 1,
                                                          currChildFrameIdx);
            currChildDfsData.resultFrameParentIdx = currFrameIdx;
            currChildDfsData.resultFrameOffsetInParent = offset;
            updateDfsData(currChildFrameIdx, dfsData);
            if (dfsData.foundEmpty){  // TODO: this relies on updateDfsData to update the foundEmpty flag according to the flag of the child.
                return dfsData;
            }
        }
    }

    if (emptyFrame && (currFrameIdx != ignoreFrameIdx)) {
        dfsData.foundEmpty = true;
        dfsData.resultFrameIdx = currFrameIdx;
        return dfsData;
    }

    return dfsData;
}

word_t handlePageFault(uint64_t virtualAddress, word_t ignoreFrameIdx) {
    auto dfsResult = handlePageFaultHelper(virtualAddress, 0, 1, 0, ignoreFrameIdx);

}

uint64_t getPhysicalAddress(uint64_t virtualAddress) {
    //TODO: does FrameIdx need to be word_t?
    word_t currFrameIdx = 0;
    //TODO: < or <=?
    for (int level = 1; level <= TABLES_DEPTH; level++) {
        uint64_t currPi = getPi(virtualAddress, level);
        word_t prevFrameIDx = currFrameIdx;
        PMread((currFrameIdx * PAGE_SIZE) + currPi, &currFrameIdx);
        if (currFrameIdx == 0) {
            currFrameIdx = handlePageFault(virtualAddress, prevFrameIDx);
        }
    }
    uint64_t offset = getOffset(virtualAddress);
    return (currFrameIdx * PAGE_SIZE) + offset;
}


void VMinitialize() {
    // TODO: this is not good
    PMwrite(0, 0);
}

int VMread(uint64_t virtualAddress, word_t *value) {
    uint64_t physicalAddress = getPhysicalAddress(virtualAddress);
    PMread(physicalAddress, value);
}

int VMwrite(uint64_t virtualAddress, word_t value) {
    uint64_t physicalAddress = getPhysicalAddress(virtualAddress);
    PMwrite(physicalAddress, value);
}


