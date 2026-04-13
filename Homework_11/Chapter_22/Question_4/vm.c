// LARRY LA - CS 4080 - HW 11
/*
Ch.22 Q4: VM now reads 16-bit local slot operands.
See lines 78-79 and 124-130.

Example:
Input: many locals, then `v299 = 42; print v299;`
Output: 42
*/

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

VM vm;

static void resetStack(void) {
  vm.stackTop = vm.stack;
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  size_t instruction = (size_t)(vm.ip - vm.chunk->code - 1);
  int line = getLine(vm.chunk, (int)instruction);
  fprintf(stderr, "[line %d] in script\n", line);
  resetStack();
}

static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static int resolveGlobalSlot(uint8_t nameConstant) {
  int slot = vm.globalSlots[nameConstant];
  if (slot != -1) return slot;

  ObjString* name = AS_STRING(vm.chunk->constants.values[nameConstant]);
  Value slotValue;
  if (!tableGet(&vm.globals, OBJ_VAL(name), &slotValue)) return -1;

  slot = (int)AS_NUMBER(slotValue);
  vm.globalSlots[nameConstant] = slot;
  return slot;
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void) {
  ObjString* b = AS_STRING(pop());
  ObjString* a = AS_STRING(pop());

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  push(OBJ_VAL(result));
}

static InterpretResult run(void) {
#define READ_BYTE() (*vm.ip++)
#define READ_SHORT() \
  (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op)                       \
  do {                                                 \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      runtimeError("Operands must be numbers.");       \
      return INTERPRET_RUNTIME_ERROR;                  \
    }                                                  \
    double b = AS_NUMBER(pop());                       \
    double a = AS_NUMBER(pop());                       \
    push(valueType(a op b));                           \
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
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint16_t slot = READ_SHORT();
        push(vm.stack[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint16_t slot = READ_SHORT();
        vm.stack[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        uint8_t nameConstant = READ_BYTE();
        int slot = resolveGlobalSlot(nameConstant);
        if (slot == -1) {
          ObjString* name = AS_STRING(vm.chunk->constants.values[nameConstant]);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        push(vm.globalValues.values[slot]);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        uint8_t nameConstant = READ_BYTE();
        ObjString* name = AS_STRING(vm.chunk->constants.values[nameConstant]);

        Value slotValue;
        if (tableGet(&vm.globals, OBJ_VAL(name), &slotValue)) {
          int slot = (int)AS_NUMBER(slotValue);
          vm.globalValues.values[slot] = peek(0);
          vm.globalSlots[nameConstant] = slot;
        } else {
          writeValueArray(&vm.globalValues, peek(0));
          int slot = vm.globalValues.count - 1;
          tableSet(&vm.globals, OBJ_VAL(name), NUMBER_VAL((double)slot));
          vm.globalSlots[nameConstant] = slot;
        }
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        uint8_t nameConstant = READ_BYTE();
        int slot = resolveGlobalSlot(nameConstant);
        if (slot == -1) {
          ObjString* name = AS_STRING(vm.chunk->constants.values[nameConstant]);
          runtimeError("Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }
        vm.globalValues.values[slot] = peek(0);
        break;
      }
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          runtimeError("Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          runtimeError("Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      case OP_RETURN:
        return INTERPRET_OK;
      default:
        return INTERPRET_RUNTIME_ERROR;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

void initVM(void) {
  vm.stackCapacity = STACK_INITIAL_CAPACITY;
  vm.stack = GROW_ARRAY(Value, NULL, 0, vm.stackCapacity);
  resetStack();
  initTable(&vm.globals);
  initValueArray(&vm.globalValues);
  vm.globalSlots = NULL;
  vm.globalSlotCount = 0;
  initTable(&vm.strings);
  vm.objects = NULL;
}

void freeVM(void) {
  freeTable(&vm.globals);
  freeValueArray(&vm.globalValues);
  FREE_ARRAY(int, vm.globalSlots, vm.globalSlotCount);
  vm.globalSlots = NULL;
  vm.globalSlotCount = 0;
  freeTable(&vm.strings);
  freeObjects();
  FREE_ARRAY(Value, vm.stack, vm.stackCapacity);
  vm.stack = NULL;
  vm.stackTop = NULL;
  vm.stackCapacity = 0;
  initTable(&vm.globals);
  initValueArray(&vm.globalValues);
  initTable(&vm.strings);
  vm.objects = NULL;
}

InterpretResult interpret(const char* source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;
  vm.globalSlotCount = vm.chunk->constants.count;
  vm.globalSlots = ALLOCATE(int, vm.globalSlotCount);
  for (int i = 0; i < vm.globalSlotCount; i++) {
    vm.globalSlots[i] = -1;
  }

  InterpretResult result = run();

  FREE_ARRAY(int, vm.globalSlots, vm.globalSlotCount);
  vm.globalSlots = NULL;
  vm.globalSlotCount = 0;
  freeChunk(&chunk);
  return result;
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
