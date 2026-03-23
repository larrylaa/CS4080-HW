#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// LARRY LA - CS 4080 - HW 8
/*
Ch.14 Q1: Added run-length encoded line metadata.
See lines 12-15, 26-28, 35.
*/
typedef struct {
  int line;
  int runLength;
} LineRun;

typedef enum {
  OP_CONSTANT,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int lineCount;
  int lineCapacity;
  LineRun* lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int getLine(Chunk* chunk, int instruction);
int addConstant(Chunk* chunk, Value value);

#endif
