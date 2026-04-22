// LARRY LA - CS 4080 - HW 12
/*
Ch.25 Q1: CallFrame stores both ObjFunction* and optional ObjClosure*.
See lines 18-22.

Example:
Input: plain function call with no captured upvalues.
Output: frame executes via function pointer without requiring closure object.
*/

#ifndef clox_vm_h
#define clox_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  ObjFunction* function;
  ObjClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frameCount;
  Value stack[STACK_MAX];
  Value* stackTop;
  Table globals;
  ValueArray globalValues;
  Table strings;
  ObjUpvalue* openUpvalues;
  Obj* objects;
} VM;

extern VM vm;

void initVM(void);
void freeVM(void);

InterpretResult interpret(const char* source);
void push(Value value);
Value pop(void);

#endif
