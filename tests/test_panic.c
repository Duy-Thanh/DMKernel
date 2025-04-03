#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/dmkernel.h"

int main(int argc, char **argv) {
    dm_context_t *ctx = NULL;
    
    // Initialize kernel
    dm_error_t error = dm_init(&ctx);
    if (error != DM_SUCCESS) {
        fprintf(stderr, "Failed to initialize kernel: %s\n", dm_error_string(error));
        return 1;
    }
    
    printf("DMKernel initialized successfully. Press Enter to trigger a kernel panic...\n");
    getchar();
    
    // Trigger kernel panic
    DM_PANIC(ctx, "This is a test kernel panic with %s and %d arguments", "string", 42);
    
    // This code should never be reached
    printf("This should never be printed\n");
    
    // Clean up
    dm_cleanup(ctx);
    
    return 0;
} 