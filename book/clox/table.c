#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75
#define ENTRY_EMPTY 0
#define ENTRY_TOMBSTONE 1
#define ENTRY_OCCUPIED 2

static uint32_t hashBits(uint64_t x) {
  x = (~x) + (x << 18);
  x = x ^ (x >> 31);
  x = x * 21;
  x = x ^ (x >> 11);
  x = x + (x << 6);
  x = x ^ (x >> 22);
  return (uint32_t)x;
}

static uint32_t hashValue(Value key) {
  if (IS_BOOL(key)) {
    return AS_BOOL(key) ? 3u : 2u;
  }
  if (IS_NIL(key)) return 1u;
  if (IS_NUMBER(key)) {
    double number = AS_NUMBER(key);
    if (number == 0) number = 0.0;
    uint64_t bits;
    memcpy(&bits, &number, sizeof(double));
    return hashBits(bits);
  }
  if (IS_STRING(key)) return AS_STRING(key)->hash;
  if (IS_OBJ(key)) return hashBits((uint64_t)(uintptr_t)AS_OBJ(key));

  return 0;
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
  uint32_t index = hashValue(key) & (capacity - 1);
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];
    if (entry->state == ENTRY_EMPTY) {
      return tombstone != NULL ? tombstone : entry;
    } else if (entry->state == ENTRY_TOMBSTONE) {
      if (tombstone == NULL) tombstone = entry;
    } else if (valuesEqual(entry->key, key)) {
      return entry;
    }

    index = (index + 1) & (capacity - 1);
  }
}

static void adjustCapacity(Table* table, int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
    entries[i].state = ENTRY_EMPTY;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->state != ENTRY_OCCUPIED) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    dest->state = ENTRY_OCCUPIED;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

bool tableGet(Table* table, Value key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->state != ENTRY_OCCUPIED) return false;

  *value = entry->value;
  return true;
}

bool tableSet(Table* table, Value key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = entry->state != ENTRY_OCCUPIED;
  if (entry->state == ENTRY_EMPTY) table->count++;

  entry->key = key;
  entry->value = value;
  entry->state = ENTRY_OCCUPIED;
  return isNewKey;
}

bool tableDelete(Table* table, Value key) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (entry->state != ENTRY_OCCUPIED) return false;

  entry->key = NIL_VAL;
  entry->value = NIL_VAL;
  entry->state = ENTRY_TOMBSTONE;
  return true;
}

void tableAddAll(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->state == ENTRY_OCCUPIED) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString* tableFindString(Table* table, const char* chars,
                           int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash & (table->capacity - 1);
  for (;;) {
    Entry* entry = &table->entries[index];
    if (entry->state == ENTRY_EMPTY) {
      return NULL;
    } else if (entry->state == ENTRY_OCCUPIED &&
               IS_STRING(entry->key)) {
      ObjString* key = AS_STRING(entry->key);
      if (key->length == length &&
          key->hash == hash &&
          memcmp(key->chars, chars, length) == 0) {
        return key;
      }
    }

    index = (index + 1) & (table->capacity - 1);
  }
}

void tableRemoveWhite(Table* table, bool currentMark) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->state == ENTRY_OCCUPIED &&
        IS_STRING(entry->key) &&
        AS_STRING(entry->key)->obj.isMarked != currentMark) {
      tableDelete(table, entry->key);
    }
  }
}

void markTable(Table* table) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->state != ENTRY_OCCUPIED) continue;
    markValue(entry->key);
    markValue(entry->value);
  }
}
