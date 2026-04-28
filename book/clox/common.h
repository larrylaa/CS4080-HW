#ifndef clox_common_h
#define clox_common_h

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

#define UINT8_COUNT (UINT8_MAX + 1)

#endif
