// LARRY LA - CS 4080 - HW 13
/*
Ch.26 Q2: Companion file for flip-bit mark strategy optimization.
See line 32.
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
void tableRemoveWhite(Table* table, bool currentMark);
void markTable(Table* table);

#endif
