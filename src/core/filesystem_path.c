#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include "../../include/core/filesystem.h"

// VFS key in context
#define DM_VFS_KEY "dm_vfs"

// Helper to get VFS from context
static dm_vfs_t* get_vfs(dm_context_t *ctx) {
    dm_value_t vfs_val;
    dm_error_t err = dm_scope_lookup(ctx, ctx->global_scope, DM_VFS_KEY, &vfs_val);
    if (err != DM_SUCCESS || vfs_val.type != DM_TYPE_OBJECT) {
        return NULL;
    }
    return (dm_vfs_t*)vfs_val.as.object;
}

// Join two paths
dm_error_t dm_path_join(dm_context_t *ctx, const char *path1, const char *path2, char **result) {
    if (ctx == NULL || path1 == NULL || path2 == NULL || result == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get path separator
    dm_vfs_t *vfs = get_vfs(ctx);
    char separator = vfs ? vfs->path_separator : '/';
    
    // Handle special cases
    if (path2[0] == '/') {
        // path2 is absolute, just return it
        *result = dm_strdup(ctx, path2);
        if (*result == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        return DM_SUCCESS;
    }
    
    // Handle empty path1
    if (strlen(path1) == 0) {
        *result = dm_strdup(ctx, path2);
        if (*result == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        return DM_SUCCESS;
    }
    
    // Check if path1 ends with separator
    bool path1_has_sep = false;
    char last_char = path1[strlen(path1) - 1];
    path1_has_sep = (last_char == '/' || last_char == '\\');
    
    // Allocate result
    *result = dm_malloc(ctx, strlen(path1) + strlen(path2) + 2);
    if (*result == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Construct result
    strcpy(*result, path1);
    
    // Add separator if needed
    if (!path1_has_sep && strlen(path1) > 0 && strlen(path2) > 0) {
        size_t len = strlen(*result);
        (*result)[len] = separator;
        (*result)[len + 1] = '\0';
    }
    
    // Append path2
    strcat(*result, path2);
    
    return DM_SUCCESS;
}

// Get absolute path
dm_error_t dm_path_absolute(dm_context_t *ctx, const char *path, char **abs_path) {
    if (ctx == NULL || path == NULL || abs_path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // If path is already absolute, just copy it
    if (path[0] == '/') {
        *abs_path = dm_strdup(ctx, path);
        if (*abs_path == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        return DM_SUCCESS;
    }
    
    // Special cases: "." and ".."
    if (strcmp(path, ".") == 0) {
        // Get working directory
        dm_error_t err = dm_vfs_get_working_dir(ctx, abs_path);
        if (err != DM_SUCCESS) {
            return err;
        }
        
        return DM_SUCCESS;
    }
    
    // Get current working directory
    char *working_dir = NULL;
    dm_error_t err = dm_vfs_get_working_dir(ctx, &working_dir);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Join working directory and path
    err = dm_path_join(ctx, working_dir, path, abs_path);
    if (err != DM_SUCCESS) {
        dm_free(ctx, working_dir);
        return err;
    }
    
    // Free working directory
    dm_free(ctx, working_dir);
    
    return DM_SUCCESS;
}

// Get basename of path
dm_error_t dm_path_basename(dm_context_t *ctx, const char *path, char **basename_out) {
    if (ctx == NULL || path == NULL || basename_out == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Use system basename, but we need to make a copy because basename can modify its argument
    char *path_copy = dm_malloc(ctx, strlen(path) + 1);
    if (path_copy == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(path_copy, path);
    
    // Get basename
    char *base = basename(path_copy);
    
    // Copy basename to result
    *basename_out = dm_malloc(ctx, strlen(base) + 1);
    if (*basename_out == NULL) {
        dm_free(ctx, path_copy);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(*basename_out, base);
    
    // Free path copy
    dm_free(ctx, path_copy);
    
    return DM_SUCCESS;
}

// Get dirname of path
dm_error_t dm_path_dirname(dm_context_t *ctx, const char *path, char **dirname_out) {
    if (ctx == NULL || path == NULL || dirname_out == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Use system dirname, but we need to make a copy because dirname can modify its argument
    char *path_copy = dm_malloc(ctx, strlen(path) + 1);
    if (path_copy == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(path_copy, path);
    
    // Get dirname
    char *dir = dirname(path_copy);
    
    // Copy dirname to result
    *dirname_out = dm_malloc(ctx, strlen(dir) + 1);
    if (*dirname_out == NULL) {
        dm_free(ctx, path_copy);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(*dirname_out, dir);
    
    // Free path copy
    dm_free(ctx, path_copy);
    
    return DM_SUCCESS;
}

// Get file extension
dm_error_t dm_path_extension(dm_context_t *ctx, const char *path, char **extension) {
    if (ctx == NULL || path == NULL || extension == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get basename first
    char *basename = NULL;
    dm_error_t err = dm_path_basename(ctx, path, &basename);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Find last dot in basename
    char *dot = strrchr(basename, '.');
    if (dot == NULL || dot == basename) {
        // No extension
        *extension = dm_malloc(ctx, 1);
        if (*extension == NULL) {
            dm_free(ctx, basename);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        (*extension)[0] = '\0';
    } else {
        // Copy extension (including the dot)
        *extension = dm_malloc(ctx, strlen(dot) + 1);
        if (*extension == NULL) {
            dm_free(ctx, basename);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        strcpy(*extension, dot);
    }
    
    // Free basename
    dm_free(ctx, basename);
    
    return DM_SUCCESS;
} 