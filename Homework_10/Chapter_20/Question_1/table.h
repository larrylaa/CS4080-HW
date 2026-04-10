// LARRY LA - CS 4080 - HW 10
/*
Ch.20 Q1: Generalized hash table keys from ObjString* to Value so numbers,
Booleans, nil, and strings can all be keys.
See lines 12-28.

Example:
Input: tableSet(&t, NUMBER_VAL(42), NUMBER_VAL(1.5));
Output: tableGet(&t, NUMBER_VAL(42), &out) -> true, out = 1.5
*/

#ifndef clox_table_h
#define clox_table_h

#include "common.h"
#include "value.h"

typedef struct {
  Value key;
  Value value;
  uint8_t state;
} Entry;

typedef struct {
  int count;
  int capacity;
  Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, Value key, Value* value);
bool tableSet(Table* table, Value key, Value value);
bool tableDelete(Table* table, Value key);
void tableAddAll(Table* from, Table* to);
ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash);

#endif
