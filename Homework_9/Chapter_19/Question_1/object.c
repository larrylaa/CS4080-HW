#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

// LARRY LA - CS 4080 - HW 9
/*
Ch.19 Q1: Switched ObjString to one contiguous allocation using a flexible
array member, removing the extra chars allocation.
See lines 30-48.

Example:
Input: "st" + "ri" + "ng"
Output: string
*/
#define ALLOCATE_OBJ(type, objectType) \
  (type*)allocateObject(sizeof(type), objectType)

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)reallocate(NULL, 0, size);
  object->type = type;
  object->next = vm.objects;
  vm.objects = object;
  return object;
}

static ObjString* allocateString(int length) {
  ObjString* string = (ObjString*)allocateObject(
      sizeof(ObjString) + (size_t)length + 1, OBJ_STRING);
  string->length = length;
  string->chars[length] = '\0';
  return string;
}

ObjString* takeString(char* chars, int length) {
  ObjString* string = allocateString(length);
  memcpy(string->chars, chars, length);
  FREE_ARRAY(char, chars, length + 1);
  return string;
}

ObjString* copyString(const char* chars, int length) {
  ObjString* string = allocateString(length);
  memcpy(string->chars, chars, length);
  return string;
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
    case OBJ_STRING:
      printf("%s", AS_CSTRING(value));
      break;
  }
}
