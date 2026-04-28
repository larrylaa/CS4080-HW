// LARRY LA - CS 4080 - HW 15
/*
Ch.30 Q1: Added global slot cache toggle.
See lines 28-29.
*/
#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef NAN_BOXING
#define NAN_BOXING
#endif

#ifndef DEBUG_PRINT_CODE
#define DEBUG_PRINT_CODE 1
#endif

#ifndef DEBUG_TRACE_EXECUTION
#define DEBUG_TRACE_EXECUTION 1
#endif

#ifndef DEBUG_STRESS_GC
#define DEBUG_STRESS_GC 0
#endif

#ifndef DEBUG_LOG_GC
#define DEBUG_LOG_GC 0
#endif

#ifndef CLOX_GLOBAL_SLOT_CACHE
#define CLOX_GLOBAL_SLOT_CACHE 1
#endif

#ifndef CLOX_SMALL_STRING_OBJ
#define CLOX_SMALL_STRING_OBJ 1
#endif

#define UINT8_COUNT (UINT8_MAX + 1)

#endif
