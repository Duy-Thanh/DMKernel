#include "../include/dmkernel.h"

// Convert error code to string
const char *dm_error_string(dm_error_t error) {
    switch (error) {
        case DM_SUCCESS:
            return "Success";
        case DM_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";
        case DM_ERROR_MEMORY_ALLOCATION:
            return "Memory allocation error";
        case DM_ERROR_FILE_IO:
            return "File I/O error";
        case DM_ERROR_NOT_FOUND:
            return "Not found";
        case DM_ERROR_ALREADY_EXISTS:
            return "Already exists";
        case DM_ERROR_PERMISSION_DENIED:
            return "Permission denied";
        case DM_ERROR_NOT_SUPPORTED:
            return "Not supported";
        case DM_ERROR_BUFFER_OVERFLOW:
            return "Buffer overflow";
        case DM_ERROR_TIMEOUT:
            return "Timeout";
        case DM_ERROR_BUSY:
            return "Resource busy";
        case DM_ERROR_WOULD_BLOCK:
            return "Operation would block";
        case DM_ERROR_INTERRUPTED:
            return "Operation interrupted";
        case DM_ERROR_DIVISION_BY_ZERO:
            return "Division by zero";
        case DM_ERROR_TYPE_MISMATCH:
            return "Type mismatch";
        case DM_ERROR_SYNTAX_ERROR:
            return "Syntax error";
        case DM_ERROR_UNDEFINED_VARIABLE:
            return "Undefined variable";
        case DM_ERROR_INDEX_OUT_OF_BOUNDS:
            return "Index out of bounds";
        case DM_ERROR_STACK_OVERFLOW:
            return "Stack overflow";
        default:
            return "Unknown error";
    }
} 