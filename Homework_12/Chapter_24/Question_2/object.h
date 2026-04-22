// LARRY LA - CS 4080 - HW 12
/*
Ch.24 Q2: Added native-function arity metadata and access macros.
See lines 18-21, 56-62, and 81.

Example:
Input: clock(1);
Output: runtime error "Expected 0 arguments but got 1."
*/

#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value)        (AS_OBJ(value)->type)

#define IS_CLOSURE(value)      isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)     isObjType(value, OBJ_FUNCTION)
#define IS_NATIVE(value)       isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)       isObjType(value, OBJ_STRING)
#define IS_UPVALUE(value)      isObjType(value, OBJ_UPVALUE)

#define AS_CLOSURE(value)      ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)     ((ObjFunction*)AS_OBJ(value))
#define AS_NATIVE_OBJ(value)   ((ObjNative*)AS_OBJ(value))
#define AS_NATIVE(value) \
    (AS_NATIVE_OBJ(value)->function)
#define AS_NATIVE_ARITY(value) (AS_NATIVE_OBJ(value)->arity)
#define AS_STRING(value)       ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)      (((ObjString*)AS_OBJ(value))->chars)
#define AS_UPVALUE(value)      ((ObjUpvalue*)AS_OBJ(value))

typedef enum {
  OBJ_CLOSURE,
  OBJ_FUNCTION,
  OBJ_NATIVE,
  OBJ_STRING,
  OBJ_UPVALUE,
} ObjType;

struct Obj {
  ObjType type;
  struct Obj* next;
};

typedef struct {
  Obj obj;
  int arity;
  int upvalueCount;
  Chunk chunk;
  ObjString* name;
} ObjFunction;

typedef struct ObjUpvalue ObjUpvalue;

typedef struct {
  Obj obj;
  ObjFunction* function;
  ObjUpvalue** upvalues;
  int upvalueCount;
} ObjClosure;

typedef bool (*NativeFn)(int argCount, Value* args, Value* result);

typedef struct {
  Obj obj;
  NativeFn function;
  int arity;
} ObjNative;

struct ObjString {
  Obj obj;
  int length;
  char* chars;
  uint32_t hash;
  bool ownsChars;
};

struct ObjUpvalue {
  Obj obj;
  Value* location;
  Value closed;
  struct ObjUpvalue* next;
};

ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction(void);
ObjNative* newNative(NativeFn function, int arity);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
ObjString* constantString(const char* chars, int length);
ObjUpvalue* newUpvalue(Value* slot);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
  return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
