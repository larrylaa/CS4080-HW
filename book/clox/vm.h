#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_INITIAL_CAPACITY 8

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

typedef struct {
  Chunk* chunk;
  uint8_t* ip;
  Value* stack;
  Value* stackTop;
  int stackCapacity;
} VM;

void initVM(void);
void freeVM(void);

InterpretResult interpret(const char* source);
void push(Value value);
Value pop(void);

#endif
