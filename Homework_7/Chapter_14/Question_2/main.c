#include "chunk.h"
#include "common.h"
#include "debug.h"

// LARRY LA - CS 4080 - HW 8
/*
Ch.14 Q2: main now calls writeConstant() helper.
See line 17.
*/
int main(int argc, const char* argv[]) {
  (void)argc;
  (void)argv;

  Chunk chunk;
  initChunk(&chunk);

  writeConstant(&chunk, 1.2, 123);

  writeChunk(&chunk, OP_RETURN, 123);

  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);
  return 0;
}
