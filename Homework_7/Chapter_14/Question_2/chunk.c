#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

// LARRY LA - CS 4080 - HW 8
/*
Ch.14 Q2: writeConstant() emits OP_CONSTANT for 1-byte indexes and
OP_CONSTANT_LONG for 24-bit indexes.
See lines 70-83.

Example input/output:
Input: 257 unique constants
Output: constant #256 encoded with OP_CONSTANT_LONG
*/
void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lineCount = 0;
  chunk->lineCapacity = 0;
  chunk->lines = NULL;
  initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(LineRun, chunk->lines, chunk->lineCapacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code,
                             oldCapacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  if (chunk->lineCount > 0 &&
      chunk->lines[chunk->lineCount - 1].line == line) {
    chunk->lines[chunk->lineCount - 1].runLength++;
  } else {
    if (chunk->lineCapacity < chunk->lineCount + 1) {
      int oldLineCapacity = chunk->lineCapacity;
      chunk->lineCapacity = GROW_CAPACITY(oldLineCapacity);
      chunk->lines = GROW_ARRAY(LineRun, chunk->lines,
                                oldLineCapacity, chunk->lineCapacity);
    }

    chunk->lines[chunk->lineCount].line = line;
    chunk->lines[chunk->lineCount].runLength = 1;
    chunk->lineCount++;
  }

  chunk->count++;
}

int getLine(Chunk* chunk, int instruction) {
  if (instruction < 0 || instruction >= chunk->count) return -1;

  int accumulated = 0;
  for (int run = 0; run < chunk->lineCount; run++) {
    accumulated += chunk->lines[run].runLength;
    if (instruction < accumulated) {
      return chunk->lines[run].line;
    }
  }

  return -1;
}

int addConstant(Chunk* chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void writeConstant(Chunk* chunk, Value value, int line) {
  int constant = addConstant(chunk, value);

  if (constant <= UINT8_MAX) {
    writeChunk(chunk, OP_CONSTANT, line);
    writeChunk(chunk, (uint8_t)constant, line);
    return;
  }

  writeChunk(chunk, OP_CONSTANT_LONG, line);
  writeChunk(chunk, (uint8_t)((constant >> 16) & 0xff), line);
  writeChunk(chunk, (uint8_t)((constant >> 8) & 0xff), line);
  writeChunk(chunk, (uint8_t)(constant & 0xff), line);
}
