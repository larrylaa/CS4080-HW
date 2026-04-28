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
static void collectAcyclicGarbage(void);
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

static int findObjectIndex(Obj** objects, int objectCount, Obj* target) {
  for (int i = 0; i < objectCount; i++) {
    if (objects[i] == target) return i;
  }
  return -1;
}

static void rcAddObjectRef(Obj** objects, int objectCount, int* refCounts,
                           Obj* target) {
  if (target == NULL) return;
  int index = findObjectIndex(objects, objectCount, target);
  if (index >= 0) refCounts[index]++;
}

static void rcAddValueRef(Obj** objects, int objectCount, int* refCounts,
                          Value value) {
  if (IS_OBJ(value)) rcAddObjectRef(objects, objectCount, refCounts, AS_OBJ(value));
}

static void rcAddTableRefs(Obj** objects, int objectCount, int* refCounts,
                           Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->state != 2) continue;
    rcAddValueRef(objects, objectCount, refCounts, entry->key);
    rcAddValueRef(objects, objectCount, refCounts, entry->value);
  }
}

static void rcAddInternalRefs(Obj** objects, int objectCount, int* refCounts,
                              Obj* object) {
  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      rcAddValueRef(objects, objectCount, refCounts, bound->receiver);
      rcAddObjectRef(objects, objectCount, refCounts, (Obj*)bound->method);
      break;
    }
    case OBJ_CLASS:
      rcAddObjectRef(objects, objectCount, refCounts,
                     (Obj*)((ObjClass*)object)->name);
      rcAddTableRefs(objects, objectCount, refCounts,
                     &((ObjClass*)object)->methods);
      break;
    case OBJ_CLOSURE: {
      ObjClosure* closure = (ObjClosure*)object;
      rcAddObjectRef(objects, objectCount, refCounts, (Obj*)closure->function);
      for (int i = 0; i < closure->upvalueCount; i++) {
        rcAddObjectRef(objects, objectCount, refCounts, (Obj*)closure->upvalues[i]);
      }
      break;
    }
    case OBJ_FUNCTION: {
      ObjFunction* function = (ObjFunction*)object;
      rcAddObjectRef(objects, objectCount, refCounts, (Obj*)function->name);
      for (int i = 0; i < function->chunk.constants.count; i++) {
        rcAddValueRef(objects, objectCount, refCounts,
                      function->chunk.constants.values[i]);
      }
      break;
    }
    case OBJ_INSTANCE: {
      ObjInstance* instance = (ObjInstance*)object;
      rcAddObjectRef(objects, objectCount, refCounts, (Obj*)instance->klass);
      rcAddTableRefs(objects, objectCount, refCounts, &instance->fields);
      break;
    }
    case OBJ_UPVALUE:
      rcAddValueRef(objects, objectCount, refCounts, ((ObjUpvalue*)object)->closed);
      break;
    case OBJ_NATIVE:
    case OBJ_STRING:
      break;
  }
}

static void collectAcyclicGarbage(void) {
  int objectCount = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    objectCount++;
  }
  if (objectCount == 0) return;

  Obj** objects = (Obj**)malloc(sizeof(Obj*) * (size_t)objectCount);
  int* refCounts = (int*)calloc((size_t)objectCount, sizeof(int));
  bool* queued = (bool*)calloc((size_t)objectCount, sizeof(bool));
  bool* reclaim = (bool*)calloc((size_t)objectCount, sizeof(bool));
  int* queue = (int*)malloc(sizeof(int) * (size_t)objectCount);
  if (objects == NULL || refCounts == NULL || queued == NULL ||
      reclaim == NULL || queue == NULL) {
    free(objects);
    free(refCounts);
    free(queued);
    free(reclaim);
    free(queue);
    return;
  }

  int at = 0;
  for (Obj* object = vm.objects; object != NULL; object = object->next) {
    objects[at++] = object;
  }

  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    rcAddValueRef(objects, objectCount, refCounts, *slot);
  }
  for (int i = 0; i < vm.frameCount; i++) {
    rcAddObjectRef(objects, objectCount, refCounts, (Obj*)vm.frames[i].closure);
  }
  for (ObjUpvalue* upvalue = vm.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    rcAddObjectRef(objects, objectCount, refCounts, (Obj*)upvalue);
  }
  rcAddTableRefs(objects, objectCount, refCounts, &vm.globals);
  for (int i = 0; i < vm.globalValues.count; i++) {
    rcAddValueRef(objects, objectCount, refCounts, vm.globalValues.values[i]);
  }
  for (int i = 0; i < objectCount; i++) {
    rcAddInternalRefs(objects, objectCount, refCounts, objects[i]);
  }

  int front = 0;
  int back = 0;
  for (int i = 0; i < objectCount; i++) {
    if (refCounts[i] == 0) {
      queue[back++] = i;
      queued[i] = true;
    }
  }

  while (front < back) {
    int index = queue[front++];
    Obj* object = objects[index];
    reclaim[index] = true;

    switch (object->type) {
      case OBJ_BOUND_METHOD: {
        ObjBoundMethod* bound = (ObjBoundMethod*)object;
        if (IS_OBJ(bound->receiver)) {
          int t = findObjectIndex(objects, objectCount, AS_OBJ(bound->receiver));
          if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
            queue[back++] = t;
            queued[t] = true;
          }
        }
        if (bound->method != NULL) {
          int t = findObjectIndex(objects, objectCount, (Obj*)bound->method);
          if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
            queue[back++] = t;
            queued[t] = true;
          }
        }
        break;
      }
      case OBJ_CLASS:
        if (((ObjClass*)object)->name != NULL) {
          int target = findObjectIndex(objects, objectCount,
                                       (Obj*)((ObjClass*)object)->name);
          if (target >= 0 && --refCounts[target] == 0 && !queued[target]) {
            queue[back++] = target;
            queued[target] = true;
          }
        }
        for (int i = 0; i < ((ObjClass*)object)->methods.capacity; i++) {
          Entry* entry = &((ObjClass*)object)->methods.entries[i];
          if (entry->state != 2) continue;
          if (IS_OBJ(entry->key)) {
            int t = findObjectIndex(objects, objectCount, AS_OBJ(entry->key));
            if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
              queue[back++] = t;
              queued[t] = true;
            }
          }
          if (IS_OBJ(entry->value)) {
            int t = findObjectIndex(objects, objectCount, AS_OBJ(entry->value));
            if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
              queue[back++] = t;
              queued[t] = true;
            }
          }
        }
        break;
      case OBJ_CLOSURE: {
        ObjClosure* closure = (ObjClosure*)object;
        if (closure->function != NULL) {
          int target = findObjectIndex(objects, objectCount, (Obj*)closure->function);
          if (target >= 0 && --refCounts[target] == 0 && !queued[target]) {
            queue[back++] = target;
            queued[target] = true;
          }
        }
        for (int i = 0; i < closure->upvalueCount; i++) {
          int target = findObjectIndex(objects, objectCount, (Obj*)closure->upvalues[i]);
          if (target >= 0 && --refCounts[target] == 0 && !queued[target]) {
            queue[back++] = target;
            queued[target] = true;
          }
        }
        break;
      }
      case OBJ_FUNCTION: {
        ObjFunction* function = (ObjFunction*)object;
        int target = findObjectIndex(objects, objectCount, (Obj*)function->name);
        if (target >= 0 && --refCounts[target] == 0 && !queued[target]) {
          queue[back++] = target;
          queued[target] = true;
        }
        for (int i = 0; i < function->chunk.constants.count; i++) {
          Value value = function->chunk.constants.values[i];
          if (!IS_OBJ(value)) continue;
          int t = findObjectIndex(objects, objectCount, AS_OBJ(value));
          if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
            queue[back++] = t;
            queued[t] = true;
          }
        }
        break;
      }
      case OBJ_INSTANCE: {
        ObjInstance* instance = (ObjInstance*)object;
        int target = findObjectIndex(objects, objectCount, (Obj*)instance->klass);
        if (target >= 0 && --refCounts[target] == 0 && !queued[target]) {
          queue[back++] = target;
          queued[target] = true;
        }
        for (int i = 0; i < instance->fields.capacity; i++) {
          Entry* entry = &instance->fields.entries[i];
          if (entry->state != 2) continue;
          if (IS_OBJ(entry->key)) {
            int t = findObjectIndex(objects, objectCount, AS_OBJ(entry->key));
            if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
              queue[back++] = t;
              queued[t] = true;
            }
          }
          if (IS_OBJ(entry->value)) {
            int t = findObjectIndex(objects, objectCount, AS_OBJ(entry->value));
            if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
              queue[back++] = t;
              queued[t] = true;
            }
          }
        }
        break;
      }
      case OBJ_UPVALUE:
        if (IS_OBJ(((ObjUpvalue*)object)->closed)) {
          int t = findObjectIndex(objects, objectCount, AS_OBJ(((ObjUpvalue*)object)->closed));
          if (t >= 0 && --refCounts[t] == 0 && !queued[t]) {
            queue[back++] = t;
            queued[t] = true;
          }
        }
        break;
      case OBJ_NATIVE:
      case OBJ_STRING:
        break;
    }
  }

  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    int index = findObjectIndex(objects, objectCount, object);
    if (index >= 0 && reclaim[index]) {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }
      freeObject(unreached);
    } else {
      previous = object;
      object = object->next;
    }
  }

  free(objects);
  free(refCounts);
  free(queued);
  free(reclaim);
  free(queue);
}

static void blackenObject(Obj* object) {
#if DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif

  switch (object->type) {
    case OBJ_BOUND_METHOD: {
      ObjBoundMethod* bound = (ObjBoundMethod*)object;
      markValue(bound->receiver);
      markObject((Obj*)bound->method);
      break;
    }
    case OBJ_CLASS: {
      ObjClass* klass = (ObjClass*)object;
      markObject((Obj*)klass->name);
      markTable(&klass->methods);
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
  markObject((Obj*)vm.initString);
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

  if (false && !hasActiveCompilerRoots()) {
    collectAcyclicGarbage();
  }

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
    case OBJ_BOUND_METHOD:
      FREE(ObjBoundMethod, object);
      break;
    case OBJ_CLASS:
      freeTable(&((ObjClass*)object)->methods);
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
