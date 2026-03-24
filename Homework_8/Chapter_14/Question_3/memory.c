#include <stdlib.h>
#include <string.h>

#include "memory.h"

// LARRY LA - CS 4080 - HW 8
/*
Ch.14 Q3 (Hardcore): Replaced malloc/free/realloc usage in reallocate()
with a custom allocator over one big heap block.
See lines 33-147.

Example input/output:
Input: reallocate(NULL, 0, 64), then reallocate(ptr, 64, 0)
Output: 64-byte block allocated, then marked free and reusable
*/
#define CLOC_HEAP_SIZE (8 * 1024 * 1024)
#define ALIGNMENT sizeof(uintptr_t)
#define MIN_SPLIT_BYTES 16

typedef struct BlockHeader {
  size_t size;
  bool isFree;
  struct BlockHeader* prev;
  struct BlockHeader* next;
} BlockHeader;

static uint8_t* heapStart = NULL;
static BlockHeader* firstBlock = NULL;
static bool heapInitialized = false;

static size_t alignSize(size_t size) {
  return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1);
}

static BlockHeader* payloadToHeader(void* payload) {
  return (BlockHeader*)((uint8_t*)payload - sizeof(BlockHeader));
}

static void* headerToPayload(BlockHeader* block) {
  return (void*)((uint8_t*)block + sizeof(BlockHeader));
}

static void splitBlock(BlockHeader* block, size_t desiredSize) {
  size_t alignedSize = alignSize(desiredSize);
  if (block->size <= alignedSize + sizeof(BlockHeader) + MIN_SPLIT_BYTES) {
    return;
  }

  BlockHeader* split = (BlockHeader*)((uint8_t*)headerToPayload(block) +
                                      alignedSize);
  split->size = block->size - alignedSize - sizeof(BlockHeader);
  split->isFree = true;
  split->prev = block;
  split->next = block->next;
  if (split->next != NULL) split->next->prev = split;

  block->size = alignedSize;
  block->next = split;
}

static void coalesceWithNext(BlockHeader* block) {
  BlockHeader* next = block->next;
  if (next == NULL || !next->isFree) return;

  block->size += sizeof(BlockHeader) + next->size;
  block->next = next->next;
  if (block->next != NULL) block->next->prev = block;
}

static void freeBlock(BlockHeader* block) {
  block->isFree = true;
  coalesceWithNext(block);
  if (block->prev != NULL && block->prev->isFree) {
    coalesceWithNext(block->prev);
  }
}

static BlockHeader* findFit(size_t size) {
  size_t alignedSize = alignSize(size);
  for (BlockHeader* block = firstBlock; block != NULL; block = block->next) {
    if (block->isFree && block->size >= alignedSize) return block;
  }
  return NULL;
}

static void* allocBlock(size_t size) {
  BlockHeader* block = findFit(size);
  if (block == NULL) return NULL;

  splitBlock(block, size);
  block->isFree = false;
  return headerToPayload(block);
}

void initMemory(void) {
  if (heapInitialized) return;

  heapStart = (uint8_t*)malloc(CLOC_HEAP_SIZE);
  if (heapStart == NULL) exit(1);

  firstBlock = (BlockHeader*)heapStart;
  firstBlock->size = CLOC_HEAP_SIZE - sizeof(BlockHeader);
  firstBlock->isFree = true;
  firstBlock->prev = NULL;
  firstBlock->next = NULL;
  heapInitialized = true;
}

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  (void)oldSize;

  if (!heapInitialized) initMemory();

  if (newSize == 0) {
    if (pointer != NULL) freeBlock(payloadToHeader(pointer));
    return NULL;
  }

  if (pointer == NULL) {
    void* allocated = allocBlock(newSize);
    if (allocated == NULL) exit(1);
    return allocated;
  }

  BlockHeader* block = payloadToHeader(pointer);
  size_t alignedNewSize = alignSize(newSize);
  if (alignedNewSize <= block->size) {
    splitBlock(block, alignedNewSize);
    return pointer;
  }

  if (block->next != NULL && block->next->isFree &&
      block->size + sizeof(BlockHeader) + block->next->size >= alignedNewSize) {
    coalesceWithNext(block);
    splitBlock(block, alignedNewSize);
    block->isFree = false;
    return pointer;
  }

  void* newPointer = allocBlock(alignedNewSize);
  if (newPointer == NULL) exit(1);

  size_t bytesToCopy = block->size < alignedNewSize ? block->size : alignedNewSize;
  memcpy(newPointer, pointer, bytesToCopy);
  freeBlock(block);
  return newPointer;
}
