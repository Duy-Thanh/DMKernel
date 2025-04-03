#ifndef DM_DEBUG_H
#define DM_DEBUG_H

#include <stdio.h>

// Debug levels
#define DM_DEBUG_NONE 0
#define DM_DEBUG_ERROR 1
#define DM_DEBUG_WARN 2
#define DM_DEBUG_INFO 3
#define DM_DEBUG_VERBOSE 4

// Set the debug level - Change this to control output
#define DM_DEBUG_LEVEL DM_DEBUG_NONE

// Debug macros
#define DM_DEBUG_ERROR_PRINT(...)
#define DM_DEBUG_WARN_PRINT(...)
#define DM_DEBUG_INFO_PRINT(...)
#define DM_DEBUG_VERBOSE_PRINT(...)

#endif /* DM_DEBUG_H */ 