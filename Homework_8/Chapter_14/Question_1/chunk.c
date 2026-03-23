#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

// LARRY LA - CS 4080 - HW 8
/*
Ch.14 Q1: Compressed line storage in writeChunk() and decode in getLine().
See lines 19-21, 27, 41-55, 60-72.

Example input/output:
Input line sequence: [7, 7, 8]
Output: getLine(0)=7, getLine(1)=7, getLine(2)=8
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
