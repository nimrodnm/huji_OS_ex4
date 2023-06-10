#include <cmath>

#include "VirtualMemory.h"
#include "PhysicalMemory.h"

#define PAGE_ADDRESS_WIDTH static_cast<uint64_t> (log2(PAGE_SIZE))
#define FRAME0_ADDRESS_WIDTH (VIRTUAL_ADDRESS_WIDTH - OFFSET_WIDTH - (PAGE_ADDRESS_WIDTH*(TABLES_DEPTH - 1)))
#define FRAME0_USED_SIZE (1LL << FRAME0_ADDRESS_WIDTH)

typedef struct {
    bool foundEmpty;
    word_t maxFrameIdx;
    uint64_t maxCyclicDist;
    word_t resultFrameIdx;
    word_t resultFrameParentIdx;
    uint64_t resultFrameOffsetInParent;
    uint64_t resultPageIdx;
} DfsData;

uint64_t GetIndexInRam(word_t frameIdx, uint64_t offset) {
    return (frameIdx * PAGE_SIZE) + offset;
}

uint64_t GetPi(uint64_t virtualAddress, int index) {
    if (index == 1) {
        uint64_t mask = FRAME0_USED_SIZE - 1;
        return (virtualAddress >> (VIRTUAL_ADDRESS_WIDTH - FRAME0_ADDRESS_WIDTH)) & mask;
    }

    // creates a mask with log2(PAGE_SIZE) bits turned on, on the right:
    uint64_t mask = PAGE_SIZE - 1;
    // shifts the virtualAddress to the right according to the index given and activates the mask:
    return (virtualAddress >> (VIRTUAL_ADDRESS_WIDTH - FRAME0_ADDRESS_WIDTH - (PAGE_ADDRESS_WIDTH * (index - 1)))) &
           mask;
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

uint64_t updateCumulativePageIdx(uint64_t cumulativePageIdx, uint64_t offset, bool isFrame0) {
    if (isFrame0){
        return (cumulativePageIdx << FRAME0_ADDRESS_WIDTH) + offset;
    }
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
 * Change the parentData according to the priority noted in the pdf
 */
void UpdateDfsData(DfsData &childData, DfsData &parentData) {
    if (childData.foundEmpty) {
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

//TODO: do we need to check if all of the frames contain table entries so we don't have page to evict? is that an error?

/**
 * Uses recursive DFS to traverse the hierarchical page table, while maintaining a DfsData struct instance that holds
 * the data needed for finding a frame in the RAM.
 * @param virtualAddress The virtual address that we want to map to the physical memory.
 * @param ignoreFrameIdx The index of the last frame that was visited before the page fault occurred. This frame will
 *                       be empty, but we'll ignore that and won't consider it as an available frame.
 * @param cumulativePageIdx An accumulated value, calculated while traversing the hierarchical page table.
 *                          When we get to a leaf in the table (the base of the recursion), it will hold the page
 *                          index in the virtual memory which represents the leaf we are at.
 * @param currDepth The current depth in the hierarchical page table.
 * @param currFrameIdx The index of the current frame that we are at.
 * @param parentFrameIdx The index of the frame that we got from in the DFS algorithm.
 * @param offsetInParent The offset in the parent frame, where the current frame index is at.
 * @return the DfsData that holds the current data we have in our DFS traversal.
 */
DfsData HandlePageFaultHelper(uint64_t virtualAddress,
                              word_t ignoreFrameIdx,
                              uint64_t cumulativePageIdx = 0,
                              int currDepth = 0,
                              word_t currFrameIdx = 0,
                              word_t parentFrameIdx = 0,
                              uint64_t offsetInParent = 0) {
    DfsData dfsData = {false, currFrameIdx, 0, 0, 0, 0, cumulativePageIdx};
    // Leaf base case:
    if (currDepth == TABLES_DEPTH) {
        dfsData.maxCyclicDist = calculateCyclicDistance(cumulativePageIdx, GetPageIdx(virtualAddress));
        dfsData.resultFrameIdx = currFrameIdx;
        dfsData.resultPageIdx = cumulativePageIdx;
        dfsData.resultFrameParentIdx = parentFrameIdx;
        dfsData.resultFrameOffsetInParent = offsetInParent;
        return dfsData;
    }

    uint64_t frameSize = (currFrameIdx == 0) ? FRAME0_USED_SIZE : PAGE_SIZE;

    bool emptyFrame = true;
    // Iterating over the entries of the current frame, recursively exploring each one of them:
    for (uint64_t offset = 0; offset < frameSize; offset++) {
        word_t currChildFrameIdx;
        PMread(GetIndexInRam(currFrameIdx, offset), &currChildFrameIdx);
        if (currChildFrameIdx != 0) {
            emptyFrame = false;
            DfsData currChildDfsData = HandlePageFaultHelper(virtualAddress,
                                                             ignoreFrameIdx,
                                                             updateCumulativePageIdx(cumulativePageIdx, offset, (currFrameIdx == 0)),
                                                             currDepth + 1,
                                                             currChildFrameIdx,
                                                             currFrameIdx,
                                                             offset);
            UpdateDfsData(currChildDfsData, dfsData);
            // trusts that UpdateDfsData updated the foundEmpty flag according to the flag of the child:
            if (dfsData.foundEmpty) {
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

/**
 * Uses the helper function to traverse the hierarchical page table in DFS order, keeping the relevant data in a
 * DfsData struct instance.
 * Then removes the link to the targetFrame from its parentFrame (if it was linked already),
 * links it to its new parent (lastBeforeFaultFrame), and initializes it.
 * @param virtualAddress The virtual address that we want to map to the physical memory.
 * @param lastBeforeFaultFrameIdx The index of the last frame that was visited before the page fault occurred.
 * @param lastBeforeFaultOffset The offset in the last frame that was visited before the page fault occurred.
 * @param level The level in the hierarchical page table where the page fault occurred.
 * @return the index of the frame in the RAM that was mapped for the faulty node.
 */
word_t HandlePageFault(uint64_t virtualAddress,
                       word_t lastBeforeFaultFrameIdx,
                       uint64_t lastBeforeFaultOffset,
                       int level) {
    auto dfsResult = HandlePageFaultHelper(virtualAddress, lastBeforeFaultFrameIdx);
    word_t targetFrameIdx;

    if (dfsResult.foundEmpty) {
        // remove the link to the empty frame from its parent:
        PMwrite(GetIndexInRam(dfsResult.resultFrameParentIdx, dfsResult.resultFrameOffsetInParent), 0);
        targetFrameIdx = dfsResult.resultFrameIdx;
    } else if ((dfsResult.maxFrameIdx + 1) < NUM_FRAMES) {
        targetFrameIdx = dfsResult.maxFrameIdx + 1;
    } else {
        // remove the link to the frame from its parent:
        PMwrite(GetIndexInRam(dfsResult.resultFrameParentIdx, dfsResult.resultFrameOffsetInParent), 0);
        PMevict(dfsResult.resultFrameIdx, dfsResult.resultPageIdx);
        targetFrameIdx = dfsResult.resultFrameIdx;
    }

    // link the empty frame to the lastBeforeFaultFrameIdx:
    PMwrite(GetIndexInRam(lastBeforeFaultFrameIdx, lastBeforeFaultOffset), targetFrameIdx);
    InitFrame(targetFrameIdx, (level == TABLES_DEPTH), GetPageIdx(virtualAddress));
    return targetFrameIdx;
}

/**
 * Gets a physical address in the ram, that is mapped to the page index in the given virtualAddress.
 * if a page fault occurs during the run of the function, the page fault handler is called to solve it.
 * @param virtualAddress a number with VIRTUAL_ADDRESS_WIDTH bits. the right most OFFSET_WIDTH bits are the offset in
 *                       the designated frame. and the VIRTUAL_ADDRESS_WIDTH-OFFSET_WIDTH left most bits are the
 *                       pageIdx in the virtual memory.
 * @return the physical address in the ram. a number with PHYSICAL_ADDRESS_WIDTH bits.
 */
uint64_t GetPhysicalAddress(uint64_t virtualAddress) {
    word_t currFrameIdx = 0;
    for (int level = 1; level <= TABLES_DEPTH; level++) {
        uint64_t currPi = GetPi(virtualAddress, level);
        word_t prevFrameIdx = currFrameIdx;
        PMread(GetIndexInRam(currFrameIdx, currPi), &currFrameIdx);
        if (currFrameIdx == 0) {
            currFrameIdx = HandlePageFault(virtualAddress, prevFrameIdx, currPi, level);
        }
    }
    uint64_t offset = GetOffset(virtualAddress);
    return GetIndexInRam(currFrameIdx, offset);
}

void VMinitialize() {
    for (int cell = 0; cell < PAGE_SIZE; cell++) {
        PMwrite(cell, 0);
    }
}

int VMread(uint64_t virtualAddress, word_t *value) {
    if ((virtualAddress >= VIRTUAL_MEMORY_SIZE) || (value == nullptr)) {
        return 0;
    }
    uint64_t physicalAddress = GetPhysicalAddress(virtualAddress);
    PMread(physicalAddress, value);
    return 1;
}

int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;
    }
    uint64_t physicalAddress = GetPhysicalAddress(virtualAddress);
    PMwrite(physicalAddress, value);
    return 1;
}


