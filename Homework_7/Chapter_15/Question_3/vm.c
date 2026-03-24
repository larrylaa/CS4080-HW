#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"

// LARRY LA - CS 4080 - HW 7
/*
Ch.15 Q3: Dynamic stack growth in push() and heap-backed stack allocation.
See lines 90-100, 109-116.

Example input/output:
Input: push 5000 values then pop 5000 values
Output: values pop in reverse order without stack overflow
*/
VM vm;

#ifndef CLOX_NEGATE_IN_PLACE
#define CLOX_NEGATE_IN_PLACE 1
#endif

static void resetStack(void) {
  vm.stackTop = vm.stack;
}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op)        \
  do {                       \
    double b = pop();        \
    double a = pop();        \
    push(a op b);            \
  } while (false)

  for (;;) {
#if DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
      printf("[ ");
      printValue(*slot);
      printf(" ]");
    }
    printf("\n");
    disassembleInstruction(vm.chunk,
                           (int)(vm.ip - vm.chunk->code));
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
      case OP_CONSTANT: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }
      case OP_CONSTANT_LONG: {
        uint32_t constant = (uint32_t)READ_BYTE() << 16;
        constant |= (uint32_t)READ_BYTE() << 8;
        constant |= (uint32_t)READ_BYTE();
        push(vm.chunk->constants.values[constant]);
        break;
      }
      case OP_ADD:      BINARY_OP(+); break;
      case OP_SUBTRACT: BINARY_OP(-); break;
      case OP_MULTIPLY: BINARY_OP(*); break;
      case OP_DIVIDE:   BINARY_OP(/); break;
      case OP_NEGATE:
#if CLOX_NEGATE_IN_PLACE
        vm.stackTop[-1] = -vm.stackTop[-1];
#else
        push(-pop());
#endif
        break;
      case OP_RETURN: {
        printValue(pop());
        printf("\n");
        return INTERPRET_OK;
      }
      default:
        return INTERPRET_RUNTIME_ERROR;
    }
  }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

void initVM(void) {
  vm.stackCapacity = STACK_INITIAL_CAPACITY;
  vm.stack = GROW_ARRAY(Value, NULL, 0, vm.stackCapacity);
  resetStack();
}

void freeVM(void) {
  FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
  vm.stack = NULL;
  vm.stackTop = NULL;
  vm.stackCapacity = 0;
}

InterpretResult interpret(Chunk* chunk) {
  vm.chunk = chunk;
  vm.ip = vm.chunk->code;
  return run();
}

void push(Value value) {
  int stackCount = (int)(vm.stackTop - vm.stack);
  if (stackCount + 1 > vm.stackCapacity) {
    int oldCapacity = vm.stackCapacity;
    vm.stackCapacity = GROW_CAPACITY(oldCapacity);
    vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
    vm.stackTop = vm.stack + stackCount;
  }

  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop(void) {
  vm.stackTop--;
  return *vm.stackTop;
}
