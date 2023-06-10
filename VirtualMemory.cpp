#pragma once

// TODO: can we use cmath?
#include <cmath>

#include "VirtualMemory.h"
#include "PhysicalMemory.h"

// TODO: is PAGE_SIZE a power of 2?
#define PAGE_ADDRESS_WIDTH static_cast<uint64_t> (log2(PAGE_SIZE))

typedef struct {
    bool foundEmpty;
    word_t maxFrameIdx;
    uint64_t maxCyclicDist;
    word_t resultFrameIdx;
    word_t resultFrameParentIdx;
    uint64_t resultFrameOffsetInParent;
    uint64_t resultPageIdx;
} DfsData;


uint64_t GetPi(uint64_t virtualAddress, int index) {
    // creates a mask with log2(PAGE_SIZE) bits turned on, on the right:
    uint64_t mask = PAGE_SIZE - 1;
    // shifts the virtualAddress to the right according to the index given and activates the mask:
    return (virtualAddress >> (VIRTUAL_ADDRESS_WIDTH - (PAGE_ADDRESS_WIDTH * index))) & mask;
}

uint64_t GetOffset(uint64_t virtualAddress) {
    // creates a mask with OFFSET_WIDTH bits turned on, on the right:
    uint64_t mask = (1LL << OFFSET_WIDTH) - 1;
    // activates the mask:
    return virtualAddress & mask;
}

uint64_t GetPageIdx(uint64_t virtualAddress) {
    return virtualAddress >> OFFSET_WIDTH;
}

uint64_t updateCumulativePageIdx(uint64_t cumulativePageIdx, uint64_t offset) {
    return (cumulativePageIdx << PAGE_ADDRESS_WIDTH) + offset;
}

uint64_t calculateCyclicDistance(uint64_t pageIdx1, uint64_t pageIdx2) {
    uint64_t dist1 = (pageIdx1 > pageIdx2) ? (pageIdx1 - pageIdx2) : (pageIdx2 - pageIdx1);
    uint64_t dist2 = NUM_PAGES - dist1;
    return (dist1 > dist2) ? dist2 : dist1;
}

void InitFrame(word_t frameIdx, bool initPage, uint64_t pageIdx) {
    if (initPage) {
        PMrestore(frameIdx, pageIdx);
    } else {
        for (uint64_t offset = 0; offset < PAGE_SIZE; offset++) {
            PMwrite((frameIdx * PAGE_SIZE) + offset, 0);
        }
    }
}

/**
 * Change the parentData according to the priority noted in the pdf:
 * 0. parentDate.maxFrameIdx = max(child.maxFrameIdx, parent.maxFrameIdx)
 * 1. if childData.foundEmpty -> set parentData.foundEmpty and all the relevant fields.
 * 2. if chil
 */
void UpdateDfsData(DfsData &childData, DfsData &parentData) {
    if (childData.foundEmpty) {
        // TODO: is it really copying?
        parentData = childData;
        return;
    }
    if (childData.maxFrameIdx > parentData.maxFrameIdx) {
        parentData.maxFrameIdx = childData.maxFrameIdx;
    }
    if (childData.maxCyclicDist > parentData.maxCyclicDist) {
        parentData.maxCyclicDist = childData.maxCyclicDist;
        parentData.resultFrameIdx = childData.resultFrameIdx;
        parentData.resultPageIdx = childData.resultPageIdx;
        parentData.resultFrameParentIdx = childData.resultFrameParentIdx;
        parentData.resultFrameOffsetInParent = childData.resultFrameOffsetInParent;
    }
}

//TODO: handle PMread/write/etc errors.
// TODO: edge case TABLE_DEPTH = 0

DfsData HandlePageFaultHelper(uint64_t virtualAddress,
                              uint64_t cumulativePageIdx,
                              int currDepth,
                              word_t currFrameIdx,
                              word_t parentFrameIdx,
                              uint64_t offsetInParent,
                              word_t ignoreFrameIdx) {
    DfsData dfsData = {false, currFrameIdx, 0, 0, 0, 0, cumulativePageIdx};
    if (currDepth == TABLES_DEPTH) {  // leaf base case
        dfsData.maxCyclicDist = calculateCyclicDistance(cumulativePageIdx, GetPageIdx(virtualAddress));
        dfsData.resultFrameIdx = currFrameIdx;
        dfsData.resultPageIdx = cumulativePageIdx;
        dfsData.resultFrameParentIdx = parentFrameIdx;
        dfsData.resultFrameOffsetInParent = offsetInParent;
        return dfsData;
    }

    bool emptyFrame = true;
    // Iterating over the entries of the current frame, recursively exploring each one of them:
    for (uint64_t offset = 0; offset < PAGE_SIZE; offset++) {
        word_t currChildFrameIdx;
        PMread((currFrameIdx * PAGE_SIZE) + offset, &currChildFrameIdx);
        if (currChildFrameIdx != 0) {
            emptyFrame = false;
            DfsData currChildDfsData = HandlePageFaultHelper(virtualAddress,
                                                             updateCumulativePageIdx(cumulativePageIdx, offset),
                                                             currDepth + 1,
                                                             currChildFrameIdx,
                                                             currFrameIdx,
                                                             offset,
                                                             ignoreFrameIdx);
            UpdateDfsData(currChildDfsData, dfsData);
            if (dfsData.foundEmpty) {  // TODO: this relies on UpdateDfsData to update the foundEmpty flag according to the flag of the child.
                return dfsData;
            }
        }
    }

    // if frameIndex is the ignoreFrameIdx, it is not considered as empty:
    if (emptyFrame && (currFrameIdx != ignoreFrameIdx)) {
        dfsData.foundEmpty = true;
        dfsData.resultFrameIdx = currFrameIdx;
        dfsData.resultFrameParentIdx = parentFrameIdx;
        dfsData.resultFrameOffsetInParent = offsetInParent;
        return dfsData;
    }

    return dfsData;
}


word_t HandlePageFault(uint64_t virtualAddress,
                       word_t lastBeforeFaultFrameIdx,
                       uint64_t lastBeforeFaultOffset,
                       int level) {
    auto dfsResult = HandlePageFaultHelper(virtualAddress, 0, 0, 0, 0, 0,lastBeforeFaultFrameIdx);
    if (dfsResult.foundEmpty) {
        // remove the link to the empty frame from its parent:
        PMwrite((dfsResult.resultFrameParentIdx * PAGE_SIZE) + dfsResult.resultFrameOffsetInParent, 0);
        // link the empty frame to the lastBeforeFaultFrameIdx:
        PMwrite((lastBeforeFaultFrameIdx * PAGE_SIZE) + lastBeforeFaultOffset, dfsResult.resultFrameIdx);
        InitFrame(dfsResult.resultFrameIdx, (level == TABLES_DEPTH), GetPageIdx(virtualAddress));
        return dfsResult.resultFrameIdx;
    } else if ((dfsResult.maxFrameIdx + 1) < NUM_FRAMES) { //TODO: NUM_PAGES or NUM_PAGES-1??
        // link the empty frame to the lastBeforeFaultFrameIdx:
        PMwrite((lastBeforeFaultFrameIdx * PAGE_SIZE) + lastBeforeFaultOffset, dfsResult.maxFrameIdx + 1);
        InitFrame(dfsResult.maxFrameIdx + 1, (level == TABLES_DEPTH), GetPageIdx((virtualAddress)));
        return dfsResult.maxFrameIdx + 1;
    } else {
        // remove the link to the frame from its parent:
        PMwrite((dfsResult.resultFrameParentIdx * PAGE_SIZE) + dfsResult.resultFrameOffsetInParent, 0);
        // link the empty frame to the lastBeforeFaultFrameIdx:
        PMwrite((lastBeforeFaultFrameIdx * PAGE_SIZE) + lastBeforeFaultOffset, dfsResult.resultFrameIdx);
        PMevict(dfsResult.resultFrameIdx, dfsResult.resultPageIdx);
        InitFrame(dfsResult.resultFrameIdx, (level == TABLES_DEPTH), GetPageIdx((virtualAddress)));
        return dfsResult.resultFrameIdx;
    }
}

//TODO: delete:
#include <cstdio>
void PrintMemory(){
    printf("---------------------------\n");
    for (int i=0; i < RAM_SIZE; i++){
        word_t value;
        PMread(i, &value);
        printf("idx:%d, value:%d\n", i, value);
    }
    printf("---------------------------\n");
}

/**
 * gets a physical address in the ram, that is mapped to the page given from the virtualAddress.
 * if a page fault occures during the run of the function, the page fault handler is called to solve it.
 * @param virtualAddress a number with VIRTUAL_ADDRESS_WIDTH bits. the right most OFFSET_WIDTH bits are the offset in
 * the designated frame. and the VIRTUAL_ADDRESS_WIDTH-OFFSET_WIDTH left most bits are the pageIdx in the virtual memory.
 * @return the physical address in the ram. a number with PHYSICAL_ADDRESS_WIDTH bits.
 */
uint64_t GetPhysicalAddress(uint64_t virtualAddress) {
    //TODO: does FrameIdx need to be word_t?
    word_t currFrameIdx = 0;
    for (int level = 1; level <= TABLES_DEPTH; level++) {
        uint64_t currPi = GetPi(virtualAddress, level);
        word_t prevFrameIdx = currFrameIdx;
        PMread((currFrameIdx * PAGE_SIZE) + currPi, &currFrameIdx);
        if (currFrameIdx == 0) {
            currFrameIdx = HandlePageFault(virtualAddress, prevFrameIdx, currPi, level);
        }
    }
    uint64_t offset = GetOffset(virtualAddress);
    return (currFrameIdx * PAGE_SIZE) + offset;
}

void VMinitialize() {
    // TODO: is this good?
    for (int cell=0; cell<PAGE_SIZE; cell++){
        PMwrite(cell, 0);
    }
}

int VMread(uint64_t virtualAddress, word_t *value) {
    uint64_t physicalAddress = GetPhysicalAddress(virtualAddress);
    PMread(physicalAddress, value);
}

int VMwrite(uint64_t virtualAddress, word_t value) {
    uint64_t physicalAddress = GetPhysicalAddress(virtualAddress);
    PMwrite(physicalAddress, value);
}


