// LARRY LA - CS 4080 - HW 13
/*
Ch.26 Q3: Companion file for RC-style acyclic pre-pass augmentation.
See line 12.
*/
#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"

ObjFunction* compile(const char* source);
void markCompilerRoots(void);
bool hasActiveCompilerRoots(void);

#endif
