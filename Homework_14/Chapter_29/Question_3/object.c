// LARRY LA - CS 4080 - HW 14
/*
Ch.29 Q3: Initialized new owner/superclass/inner tables.
See lines 31-32 and 49-51.
*/
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->isMarked = false;
  object->next = vm.objects;
  vm.objects = object;
#if DEBUG_LOG_GC
  printf("%p allocate %zu for %d\n", (void*)object, size, type);
#endif
  return object;
}

ObjClosure* newClosure(ObjFunction* function) {
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = (ObjClosure*)allocateObject(sizeof(ObjClosure),
                                                    OBJ_CLOSURE);
  closure->function = function;
  closure->ownerClass = NULL;
  closure->methodName = NULL;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
  ObjBoundMethod* bound =
      (ObjBoundMethod*)allocateObject(sizeof(ObjBoundMethod), OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjClass* newClass(ObjString* name) {
  ObjClass* klass = (ObjClass*)allocateObject(sizeof(ObjClass), OBJ_CLASS);
  klass->name = name;
  klass->superclass = NULL;
  initTable(&klass->methods);
  initTable(&klass->innerMethods);
  klass->initializer = NIL_VAL;
  return klass;
}

ObjFunction* newFunction(void) {
  ObjFunction* function = (ObjFunction*)allocateObject(sizeof(ObjFunction),
                                                       OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjInstance* newInstance(ObjClass* klass) {
  ObjInstance* instance = (ObjInstance*)allocateObject(sizeof(ObjInstance),
                                                       OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjNative* newNative(NativeFn function, int arity) {
  ObjNative* native = (ObjNative*)allocateObject(sizeof(ObjNative), OBJ_NATIVE);
  native->function = function;
  native->arity = arity;
  return native;
}

static uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

static ObjString* allocateString(char* chars, int length, uint32_t hash,
                                 bool ownsChars) {
  ObjString* string = (ObjString*)allocateObject(sizeof(ObjString), OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  string->ownsChars = ownsChars;
  push(OBJ_VAL(string));
  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
  pop();
  return string;
}

ObjString* takeString(char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }

  return allocateString(chars, length, hash, true);
}

ObjString* copyString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;

  char* heapChars = ALLOCATE(char, length + 1);
  memcpy(heapChars, chars, length);
  heapChars[length] = '\0';
  return allocateString(heapChars, length, hash, true);
}

ObjString* constantString(const char* chars, int length) {
  return copyString(chars, length);
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = (ObjUpvalue*)allocateObject(sizeof(ObjUpvalue),
                                                    OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_BOUND_METHOD:
      printFunction(AS_BOUND_METHOD(value)->method->function);
      break;
    case OBJ_CLASS:
      printf("%s", AS_CLASS(value)->name->chars);
      break;
    case OBJ_CLOSURE:
      printFunction(AS_CLOSURE(value)->function);
      break;
    case OBJ_FUNCTION:
      printFunction(AS_FUNCTION(value));
      break;
    case OBJ_INSTANCE:
      printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
      break;
    case OBJ_NATIVE:
      printf("<native fn>");
      break;
    case OBJ_STRING: {
      ObjString* string = AS_STRING(value);
      printf("%.*s", string->length, string->chars);
      break;
    }
    case OBJ_UPVALUE:
      printf("upvalue");
      break;
  }
}
