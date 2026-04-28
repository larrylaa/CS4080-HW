// LARRY LA - CS 4080 - HW 13
/*
Ch.26 Q2: Replaced sweep-time unmark clearing with flip-bit mark strategy.
See lines 170-188, 269-285, and 290-300.

Example:
Input: run with stress GC enabled on closure/string-heavy script.
Output: same program output, with fewer writes to live object headers.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "memory.h"
#include "table.h"
#include "vm.h"

#define CLOC_HEAP_SIZE (8 * 1024 * 1024)
#define ALIGNMENT sizeof(uintptr_t)
#define MIN_SPLIT_BYTES 16
#define GC_HEAP_GROW_FACTOR 2

typedef struct BlockHeader {
  size_t size;
  bool isFree;
  struct BlockHeader* prev;
  struct BlockHeader* next;
} BlockHeader;

static uint8_t* heapStart = NULL;
static BlockHeader* firstBlock = NULL;
static bool heapInitialized = false;

static void freeObject(Obj* object);
static void markRoots(void);
static void traceReferences(void);
static void sweep(void);
static void blackenObject(Obj* object);
static void markArray(ValueArray* array);

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
  if (!heapInitialized) initMemory();

  vm.bytesAllocated += newSize - oldSize;

  if (newSize > oldSize) {
#if DEBUG_STRESS_GC
    collectGarbage();
#endif

    if (vm.bytesAllocated > vm.nextGC) {
      collectGarbage();
    }
  }

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

void markObject(Obj* object) {
  if (object == NULL) return;
  if (object->isMarked == vm.currentMark) return;

#if DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  object->isMarked = vm.currentMark;
  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
    if (vm.grayStack == NULL) exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj* object) {
#if DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      markObject((Obj*)klass->name);
      break;
    }
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      markObject((Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        markObject((Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      markObject((Obj*)function->name);
      markArray(&function->chunk.constants);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      markObject((Obj*)instance->klass);
      markTable(&instance->fields);
      break;
    }
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
    case OBJ_UPVALUE:
      markValue(((ObjUpvalue*)object)->closed);
      break;
  }
}

static void markRoots(void) {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj*)vm.frames[i].closure);
  }

  for (ObjUpvalue* upvalue = vm.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj*)upvalue);
  }

  markTable(&vm.globals);
  markArray(&vm.globalValues);
  markCompilerRoots();
}

static void traceReferences(void) {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

static void sweep(void) {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked == vm.currentMark) {
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}

void collectGarbage(void) {
#if DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif

  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings, vm.currentMark);
  sweep();
  vm.currentMark = !vm.currentMark;

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#if DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated,
         vm.nextGC);
#endif
}

void freeObjects(void) {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }

  free(vm.grayStack);
}

static void freeObject(Obj* object) {
#if DEBUG_LOG_GC
  printf("%p free type %d\n", (void*)object, object->type);
#endif

  switch (object->type) {
    case OBJ_CLASS:
      FREE(ObjClass, object);
      break;
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
      FREE(ObjClosure, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      freeChunk(&function->chunk);
      FREE(ObjFunction, object);
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      freeTable(&instance->fields);
      FREE(ObjInstance, object);
      break;
    }
    case OBJ_NATIVE:
      FREE(ObjNative, object);
      break;
    case OBJ_STRING: {
      ObjString* string = (ObjString*)object;
      if (string->ownsChars) {
        FREE_ARRAY(char, string->chars, string->length + 1);
      }
      FREE(ObjString, object);
      break;
    }
    case OBJ_UPVALUE:
      FREE(ObjUpvalue, object);
      break;
  }
}
