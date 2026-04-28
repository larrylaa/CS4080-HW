// LARRY LA - CS 4080 - HW 13
/*
Ch.26 Q2: Companion file for flip-bit mark strategy optimization.
See line 628.
*/
#include <stdarg.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#ifndef CLOX_REGISTER_IP_OPT
#define CLOX_REGISTER_IP_OPT 1
#endif

#ifndef CLOX_NATIVE_SAFE_CALLS
#define CLOX_NATIVE_SAFE_CALLS 1
#endif

#ifndef CLOX_WRAP_ALL_FUNCTIONS
#define CLOX_WRAP_ALL_FUNCTIONS 0
#endif

VM vm;

static void resetStack(void) {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
  vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->function;
    size_t instruction = (size_t)(frame->ip - function->chunk.code - 1);
    fprintf(stderr, "[line %d] in ", getLine(&function->chunk, (int)instruction));
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static int resolveGlobalSlot(ObjString* name) {
  Value slotValue;
  if (!tableGet(&vm.globals, OBJ_VAL(name), &slotValue)) return -1;
  return (int)AS_NUMBER(slotValue);
}

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(void) {
  ObjString* b = AS_STRING(peek(0));
  ObjString* a = AS_STRING(peek(1));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjString* result = takeString(chars, length);
  pop();
  pop();
  push(OBJ_VAL(result));
}

static ObjUpvalue* captureUpvalue(Value* local) {
  ObjUpvalue* prevUpvalue = NULL;
  ObjUpvalue* upvalue = vm.openUpvalues;
  while (upvalue != NULL && upvalue->location > local) {
    prevUpvalue = upvalue;
    upvalue = upvalue->next;
  }

  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjUpvalue* createdUpvalue = newUpvalue(local);
  createdUpvalue->next = upvalue;

  if (prevUpvalue == NULL) {
    vm.openUpvalues = createdUpvalue;
  } else {
    prevUpvalue->next = createdUpvalue;
  }

  return createdUpvalue;
}

static void closeUpvalues(Value* last) {
  while (vm.openUpvalues != NULL &&
         vm.openUpvalues->location >= last) {
    ObjUpvalue* upvalue = vm.openUpvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm.openUpvalues = upvalue->next;
  }
}

static bool callFunction(ObjFunction* function, ObjClosure* closure,
                         int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.", function->arity,
                 argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->closure = closure;
  frame->ip = function->chunk.code;
  frame->slots = vm.stackTop - argCount - 1;
  return true;
}

static bool callClosure(ObjClosure* closure, int argCount) {
  return callFunction(closure->function, closure, argCount);
}

static bool callPlainFunction(ObjFunction* function, int argCount) {
  return callFunction(function, NULL, argCount);
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
      case OBJ_CLASS: {
        ObjClass* klass = AS_CLASS(callee);
        vm.stackTop[-argCount - 1] = OBJ_VAL(newInstance(klass));
        return true;
      }
      case OBJ_CLOSURE:
        return callClosure(AS_CLOSURE(callee), argCount);
      case OBJ_FUNCTION:
        return callPlainFunction(AS_FUNCTION(callee), argCount);
      case OBJ_NATIVE: {
        ObjNative* native = AS_NATIVE_OBJ(callee);
        if (CLOX_NATIVE_SAFE_CALLS && argCount != native->arity) {
          runtimeError("Expected %d arguments but got %d.", native->arity,
                       argCount);
          return false;
        }

        Value result;
        if (CLOX_NATIVE_SAFE_CALLS &&
            !native->function(argCount, vm.stackTop - argCount, &result)) {
          return false;
        }
        if (!CLOX_NATIVE_SAFE_CALLS) {
          native->function(argCount, vm.stackTop - argCount, &result);
        }
        vm.stackTop -= argCount + 1;
        push(result);
        return true;
      }
      default:
        break;
    }
  }

  runtimeError("Can only call functions and classes.");
  return false;
}

static InterpretResult run(void) {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  #if CLOX_REGISTER_IP_OPT
  register uint8_t* ip = frame->ip;
  #endif

  #if CLOX_REGISTER_IP_OPT
  #define READ_BYTE() (*ip++)
  #define READ_SHORT() \
    (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
  #define STORE_IP() (frame->ip = ip)
  #define LOAD_FRAME() \
    do { \
      frame = &vm.frames[vm.frameCount - 1]; \
      ip = frame->ip; \
    } while (false)
  #define ADD_IP_OFFSET(offset) (ip += (offset))
  #define SUB_IP_OFFSET(offset) (ip -= (offset))
  #define IP_OFFSET() ((int)(ip - frame->function->chunk.code))
  #else
  #define READ_BYTE() (*frame->ip++)
  #define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
  #define STORE_IP() ((void)0)
  #define LOAD_FRAME() \
    do { \
      frame = &vm.frames[vm.frameCount - 1]; \
    } while (false)
  #define ADD_IP_OFFSET(offset) (frame->ip += (offset))
  #define SUB_IP_OFFSET(offset) (frame->ip -= (offset))
  #define IP_OFFSET() ((int)(frame->ip - frame->function->chunk.code))
  #endif

#define READ_CONSTANT() \
  (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define RUNTIME_ERROR(...) \
  do { \
    STORE_IP(); \
    runtimeError(__VA_ARGS__); \
    return INTERPRET_RUNTIME_ERROR; \
  } while (false)
#define BINARY_OP(valueType, op)                       \
  do {                                                 \
    if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
      RUNTIME_ERROR("Operands must be numbers.");      \
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
    disassembleInstruction(&frame->function->chunk,
                           IP_OFFSET());
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
        push(frame->function->chunk.constants.values[constant]);
        break;
      }
      case OP_NIL: push(NIL_VAL); break;
      case OP_TRUE: push(BOOL_VAL(true)); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_POP: pop(); break;
      case OP_GET_LOCAL: {
        uint16_t slot = READ_SHORT();
        push(frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        uint16_t slot = READ_SHORT();
        frame->slots[slot] = peek(0);
        break;
      }
      case OP_GET_GLOBAL: {
        ObjString* name = READ_STRING();
        int slot = resolveGlobalSlot(name);
        if (slot == -1) {
          RUNTIME_ERROR("Undefined variable '%s'.", name->chars);
        }
        push(vm.globalValues.values[slot]);
        break;
      }
      case OP_DEFINE_GLOBAL: {
        ObjString* name = READ_STRING();
        Value slotValue;
        if (tableGet(&vm.globals, OBJ_VAL(name), &slotValue)) {
          int slot = (int)AS_NUMBER(slotValue);
          vm.globalValues.values[slot] = peek(0);
        } else {
          writeValueArray(&vm.globalValues, peek(0));
          int slot = vm.globalValues.count - 1;
          tableSet(&vm.globals, OBJ_VAL(name), NUMBER_VAL((double)slot));
        }
        pop();
        break;
      }
      case OP_SET_GLOBAL: {
        ObjString* name = READ_STRING();
        int slot = resolveGlobalSlot(name);
        if (slot == -1) {
          RUNTIME_ERROR("Undefined variable '%s'.", name->chars);
        }
        vm.globalValues.values[slot] = peek(0);
        break;
      }
      case OP_GET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        push(*frame->closure->upvalues[slot]->location);
        break;
      }
      case OP_SET_UPVALUE: {
        uint8_t slot = READ_BYTE();
        *frame->closure->upvalues[slot]->location = peek(0);
        break;
      }
      case OP_GET_PROPERTY: {
        if (!IS_INSTANCE(peek(0))) {
          RUNTIME_ERROR("Only instances have properties.");
        }

        ObjInstance* instance = AS_INSTANCE(peek(0));
        ObjString* name = READ_STRING();
        Value value;
        if (tableGet(&instance->fields, OBJ_VAL(name), &value)) {
          pop();
          push(value);
          break;
        }
        pop();
        push(NIL_VAL);
        break;
      }
      case OP_SET_PROPERTY: {
        if (!IS_INSTANCE(peek(1))) {
          RUNTIME_ERROR("Only instances have fields.");
        }

        ObjInstance* instance = AS_INSTANCE(peek(1));
        tableSet(&instance->fields, OBJ_VAL(READ_STRING()), peek(0));
        Value value = pop();
        pop();
        push(value);
        break;
      }
      case OP_PRINT: {
        printValue(pop());
        printf("\n");
        break;
      }
      case OP_JUMP: {
        uint16_t offset = READ_SHORT();
        ADD_IP_OFFSET(offset);
        break;
      }
      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();
        if (isFalsey(peek(0))) ADD_IP_OFFSET(offset);
        break;
      }
      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        SUB_IP_OFFSET(offset);
        break;
      }
      case OP_EQUAL: {
        Value b = pop();
        Value a = pop();
        push(BOOL_VAL(valuesEqual(a, b)));
        break;
      }
      case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS: BINARY_OP(BOOL_VAL, <); break;
      case OP_ADD: {
        if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
          concatenate();
        } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
          double b = AS_NUMBER(pop());
          double a = AS_NUMBER(pop());
          push(NUMBER_VAL(a + b));
        } else {
          RUNTIME_ERROR("Operands must be two numbers or two strings.");
        }
        break;
      }
      case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIVIDE: BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT:
        push(BOOL_VAL(isFalsey(pop())));
        break;
      case OP_NEGATE:
        if (!IS_NUMBER(peek(0))) {
          RUNTIME_ERROR("Operand must be a number.");
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      case OP_CALL: {
        int argCount = READ_BYTE();
        STORE_IP();
        if (!callValue(peek(argCount), argCount)) {
          return INTERPRET_RUNTIME_ERROR;
        }
        LOAD_FRAME();
        break;
      }
      case OP_CLASS:
        push(OBJ_VAL(newClass(READ_STRING())));
        break;
      case OP_CLOSURE: {
        ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
        if (!CLOX_WRAP_ALL_FUNCTIONS && function->upvalueCount == 0) {
          push(OBJ_VAL(function));
        } else {
          ObjClosure* closure = newClosure(function);
          push(OBJ_VAL(closure));
          for (int i = 0; i < closure->upvalueCount; i++) {
            uint8_t isLocal = READ_BYTE();
            uint8_t index = READ_BYTE();
            if (isLocal) {
              closure->upvalues[i] = captureUpvalue(frame->slots + index);
            } else {
              closure->upvalues[i] = frame->closure->upvalues[index];
            }
          }
        }
        break;
      }
      case OP_CLOSE_UPVALUE:
        closeUpvalues(vm.stackTop - 1);
        pop();
        break;
      case OP_CLOSE_UPVALUE_AT: {
        uint16_t slot = READ_SHORT();
        closeUpvalues(frame->slots + slot);
        break;
      }
      case OP_RETURN: {
        Value result = pop();
        closeUpvalues(frame->slots);
        vm.frameCount--;
        if (vm.frameCount == 0) {
          pop();
          return INTERPRET_OK;
        }

        vm.stackTop = frame->slots;
        push(result);
        LOAD_FRAME();
        break;
      }
      default:
        return INTERPRET_RUNTIME_ERROR;
    }
  }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef RUNTIME_ERROR
#undef STORE_IP
#undef LOAD_FRAME
#undef ADD_IP_OFFSET
#undef SUB_IP_OFFSET
#undef IP_OFFSET
#undef BINARY_OP
}

static void defineNative(const char* name, NativeFn function, int arity) {
  push(OBJ_VAL(copyString(name, (int)strlen(name))));
  push(OBJ_VAL(newNative(function, arity)));

  Value slotValue;
  if (tableGet(&vm.globals, vm.stack[0], &slotValue)) {
    int slot = (int)AS_NUMBER(slotValue);
    vm.globalValues.values[slot] = vm.stack[1];
  } else {
    writeValueArray(&vm.globalValues, vm.stack[1]);
    int slot = vm.globalValues.count - 1;
    tableSet(&vm.globals, vm.stack[0], NUMBER_VAL((double)slot));
  }

  pop();
  pop();
}

static bool clockNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  (void)args;
  *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
  return true;
}

static bool sqrtNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_NUMBER(args[0])) {
    runtimeError("sqrt() expects a number.");
    return false;
  }

  double value = AS_NUMBER(args[0]);
  if (value < 0) {
    runtimeError("sqrt() domain error: negative input.");
    return false;
  }

  *result = NUMBER_VAL(sqrt(value));
  return true;
}

static bool absNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_NUMBER(args[0])) {
    runtimeError("abs() expects a number.");
    return false;
  }

  *result = NUMBER_VAL(fabs(AS_NUMBER(args[0])));
  return true;
}

static bool lenNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_STRING(args[0])) {
    runtimeError("len() expects a string.");
    return false;
  }

  *result = NUMBER_VAL((double)AS_STRING(args[0])->length);
  return true;
}

static bool typeNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  const char* name = NULL;

  switch (args[0].type) {
    case VAL_BOOL: name = "bool"; break;
    case VAL_NIL: name = "nil"; break;
    case VAL_NUMBER: name = "number"; break;
    case VAL_OBJ:
      switch (OBJ_TYPE(args[0])) {
        case OBJ_CLASS: name = "class"; break;
        case OBJ_STRING: name = "string"; break;
        case OBJ_INSTANCE: name = "instance"; break;
        case OBJ_CLOSURE:
        case OBJ_FUNCTION: name = "function"; break;
        case OBJ_NATIVE: name = "native"; break;
        case OBJ_UPVALUE: name = "upvalue"; break;
      }
      break;
  }

  *result = OBJ_VAL(copyString(name, (int)strlen(name)));
  return true;
}

static bool hasFieldNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
    runtimeError("hasField(instance, name) expects (instance, string).");
    return false;
  }
  Value value;
  bool exists = tableGet(&AS_INSTANCE(args[0])->fields, args[1], &value);
  *result = BOOL_VAL(exists);
  return true;
}

static bool getFieldNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
    runtimeError("getField(instance, name) expects (instance, string).");
    return false;
  }
  Value value;
  if (tableGet(&AS_INSTANCE(args[0])->fields, args[1], &value)) {
    *result = value;
  } else {
    *result = NIL_VAL;
  }
  return true;
}

static bool setFieldNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
    runtimeError("setField(instance, name, value) expects (instance, string, value).");
    return false;
  }
  tableSet(&AS_INSTANCE(args[0])->fields, args[1], args[2]);
  *result = args[2];
  return true;
}

static bool delFieldNative(int argCount, Value* args, Value* result) {
  (void)argCount;
  if (!IS_INSTANCE(args[0]) || !IS_STRING(args[1])) {
    runtimeError("delField(instance, name) expects (instance, string).");
    return false;
  }
  bool removed = tableDelete(&AS_INSTANCE(args[0])->fields, args[1]);
  *result = BOOL_VAL(removed);
  return true;
}

void initVM(void) {
  resetStack();
  initTable(&vm.globals);
  initValueArray(&vm.globalValues);
  initTable(&vm.strings);
  vm.bytesAllocated = 0;
  vm.nextGC = 1024 * 1024;
  vm.currentMark = true;
  vm.objects = NULL;
  vm.grayCount = 0;
  vm.grayCapacity = 0;
  vm.grayStack = NULL;

  defineNative("clock", clockNative, 0);
  defineNative("sqrt", sqrtNative, 1);
  defineNative("abs", absNative, 1);
  defineNative("len", lenNative, 1);
  defineNative("type", typeNative, 1);
  defineNative("hasField", hasFieldNative, 2);
  defineNative("getField", getFieldNative, 2);
  defineNative("setField", setFieldNative, 3);
  defineNative("delField", delFieldNative, 2);
}

void freeVM(void) {
  freeTable(&vm.globals);
  freeValueArray(&vm.globalValues);
  freeTable(&vm.strings);
  freeObjects();
}

InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  if (CLOX_WRAP_ALL_FUNCTIONS || function->upvalueCount > 0) {
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    if (!callClosure(closure, 0)) {
      return INTERPRET_RUNTIME_ERROR;
    }
  } else if (!callPlainFunction(function, 0)) {
    return INTERPRET_RUNTIME_ERROR;
  }

  return run();
}

void push(Value value) {
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop(void) {
  vm.stackTop--;
  return *vm.stackTop;
}
