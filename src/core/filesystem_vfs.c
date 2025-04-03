#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>  // For PATH_MAX
#include "../../include/core/filesystem.h"

// VFS key in context
#define DM_VFS_KEY "dm_vfs"

// Helper functions

// Helper to get VFS from context
dm_vfs_t* get_vfs(dm_context_t *ctx) {
    if (ctx == NULL || ctx->global_scope == NULL) {
        return NULL;
    }
    
    dm_value_t vfs_val;
    dm_error_t err = dm_scope_lookup(ctx, ctx->global_scope, DM_VFS_KEY, &vfs_val);
    if (err != DM_SUCCESS) {
        return NULL;
    }
    
    if (vfs_val.type != DM_TYPE_OBJECT) {
        return NULL;
    }
    
    return (dm_vfs_t*)vfs_val.as.object;
}

// Initialize VFS
dm_error_t dm_vfs_init(dm_context_t *ctx) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Create VFS structure
    dm_vfs_t *vfs = dm_malloc(ctx, sizeof(dm_vfs_t));
    if (vfs == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize VFS
    vfs->mounts = NULL;
#ifdef _WIN32
    vfs->path_separator = '\\';
#else
    vfs->path_separator = '/';
#endif
    
    // Set working directory to current directory
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        dm_free(ctx, vfs);
        return DM_ERROR_FILE_IO;
    }
    
    vfs->working_dir = dm_malloc(ctx, strlen(cwd) + 1);
    if (vfs->working_dir == NULL) {
        dm_free(ctx, vfs);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(vfs->working_dir, cwd);
    
    // Mount root to current directory
    dm_error_t err = dm_vfs_mount(ctx, "/", cwd);
    if (err != DM_SUCCESS) {
        dm_free(ctx, vfs->working_dir);
        dm_free(ctx, vfs);
        return err;
    }
    
    // Store VFS in context
    dm_value_t vfs_val;
    vfs_val.type = DM_TYPE_OBJECT;
    vfs_val.as.object = (dm_object_t*)vfs;
    
    err = dm_scope_define(ctx, ctx->global_scope, DM_VFS_KEY, vfs_val);
    if (err != DM_SUCCESS) {
        // Clean up
        dm_vfs_unmount(ctx, "/");
        dm_free(ctx, vfs->working_dir);
        dm_free(ctx, vfs);
        return err;
    }
    
    return DM_SUCCESS;
}

// Clean up VFS
dm_error_t dm_vfs_cleanup(dm_context_t *ctx) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get VFS
    dm_vfs_t *vfs = get_vfs(ctx);
    if (vfs == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Free mount points
    dm_vfs_entry_t *entry = vfs->mounts;
    while (entry != NULL) {
        dm_vfs_entry_t *next = entry->next;
        
        // Free name and path
        if (entry->name != NULL) {
            dm_free(ctx, entry->name);
        }
        if (entry->real_path != NULL) {
            dm_free(ctx, entry->real_path);
        }
        
        // Free entry
        dm_free(ctx, entry);
        
        entry = next;
    }
    
    // Free working directory
    if (vfs->working_dir != NULL) {
        dm_free(ctx, vfs->working_dir);
    }
    
    // Free VFS
    dm_free(ctx, vfs);
    
    return DM_SUCCESS;
}

// Mount a real path to a virtual path
dm_error_t dm_vfs_mount(dm_context_t *ctx, const char *mount_point, const char *real_path) {
    if (ctx == NULL || mount_point == NULL || real_path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get VFS
    dm_vfs_t *vfs = get_vfs(ctx);
    if (vfs == NULL) {
        // First mount, just create a new entry directly
        dm_vfs_entry_t *new_entry = dm_malloc(ctx, sizeof(dm_vfs_entry_t));
        if (new_entry == NULL) {
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        // Copy name
        new_entry->name = dm_malloc(ctx, strlen(mount_point) + 1);
        if (new_entry->name == NULL) {
            dm_free(ctx, new_entry);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        strcpy(new_entry->name, mount_point);
        
        // Copy real path
        new_entry->real_path = dm_malloc(ctx, strlen(real_path) + 1);
        if (new_entry->real_path == NULL) {
            dm_free(ctx, new_entry->name);
            dm_free(ctx, new_entry);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        strcpy(new_entry->real_path, real_path);
        
        // No next entry
        new_entry->next = NULL;
        
        // Create a new VFS if needed
        vfs = dm_malloc(ctx, sizeof(dm_vfs_t));
        if (vfs == NULL) {
            dm_free(ctx, new_entry->real_path);
            dm_free(ctx, new_entry->name);
            dm_free(ctx, new_entry);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        // Initialize fields
        vfs->mounts = new_entry;
        vfs->working_dir = dm_malloc(ctx, strlen(real_path) + 1);
        if (vfs->working_dir == NULL) {
            dm_free(ctx, new_entry->real_path);
            dm_free(ctx, new_entry->name);
            dm_free(ctx, new_entry);
            dm_free(ctx, vfs);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        strcpy(vfs->working_dir, real_path);
        
        // Set path separator
        #ifdef _WIN32
        vfs->path_separator = '\\';
        #else
        vfs->path_separator = '/';
        #endif
        
        // Create value for scope
        dm_value_t vfs_val;
        vfs_val.type = DM_TYPE_OBJECT;
        vfs_val.as.object = (dm_object_t*)vfs;
        
        // Store in context
        dm_error_t err = dm_scope_define(ctx, ctx->global_scope, DM_VFS_KEY, vfs_val);
        if (err != DM_SUCCESS) {
            return err;
        }
        return err;
    }
    
    // Check if mount point already exists
    dm_vfs_entry_t *entry = vfs->mounts;
    while (entry != NULL) {
        if (strcmp(entry->name, mount_point) == 0) {
            // Update real path
            char *new_real_path = dm_malloc(ctx, strlen(real_path) + 1);
            if (new_real_path == NULL) {
                return DM_ERROR_MEMORY_ALLOCATION;
            }
            strcpy(new_real_path, real_path);
            
            // Free old path
            dm_free(ctx, entry->real_path);
            
            // Set new path
            entry->real_path = new_real_path;
            
            return DM_SUCCESS;
        }
        entry = entry->next;
    }
    
    // Create new mount point
    dm_vfs_entry_t *new_entry = dm_malloc(ctx, sizeof(dm_vfs_entry_t));
    if (new_entry == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Copy name
    new_entry->name = dm_malloc(ctx, strlen(mount_point) + 1);
    if (new_entry->name == NULL) {
        dm_free(ctx, new_entry);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(new_entry->name, mount_point);
    
    // Copy real path
    new_entry->real_path = dm_malloc(ctx, strlen(real_path) + 1);
    if (new_entry->real_path == NULL) {
        dm_free(ctx, new_entry->name);
        dm_free(ctx, new_entry);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    strcpy(new_entry->real_path, real_path);
    
    // Add to linked list
    new_entry->next = vfs->mounts;
    vfs->mounts = new_entry;
    
    return DM_SUCCESS;
}

// Unmount a virtual path
dm_error_t dm_vfs_unmount(dm_context_t *ctx, const char *mount_point) {
    if (ctx == NULL || mount_point == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get VFS
    dm_vfs_t *vfs = get_vfs(ctx);
    if (vfs == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Find mount point
    dm_vfs_entry_t *entry = vfs->mounts;
    dm_vfs_entry_t *prev = NULL;
    
    while (entry != NULL) {
        if (strcmp(entry->name, mount_point) == 0) {
            // Remove from linked list
            if (prev == NULL) {
                vfs->mounts = entry->next;
            } else {
                prev->next = entry->next;
            }
            
            // Free name and path
            dm_free(ctx, entry->name);
            dm_free(ctx, entry->real_path);
            
            // Free entry
            dm_free(ctx, entry);
            
            return DM_SUCCESS;
        }
        
        prev = entry;
        entry = entry->next;
    }
    
    // Mount point not found
    return DM_ERROR_INVALID_ARGUMENT;
}

// Resolve a virtual path to a real path
dm_error_t dm_vfs_resolve_path(dm_context_t *ctx, const char *virtual_path, char **real_path) {
    if (ctx == NULL || virtual_path == NULL || real_path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get VFS
    dm_vfs_t *vfs = get_vfs(ctx);
    if (vfs == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Handle absolute vs. relative paths
    const char *path_to_resolve = virtual_path;
    char *abs_path = NULL;
    
    if (virtual_path[0] != '/') {
        // Relative path, prepend working directory
        dm_error_t err = dm_path_join(ctx, vfs->working_dir, virtual_path, &abs_path);
        if (err != DM_SUCCESS) {
            return err;
        }
        path_to_resolve = abs_path;
    }
    
    // Find longest matching mount point
    dm_vfs_entry_t *entry = vfs->mounts;
    dm_vfs_entry_t *best_match = NULL;
    size_t best_match_len = 0;
    
    while (entry != NULL) {
        size_t mount_len = strlen(entry->name);
        
        // Check if this mount point is a prefix of the path
        if (strncmp(path_to_resolve, entry->name, mount_len) == 0) {
            // Make sure it's a complete directory match
            if (path_to_resolve[mount_len] == '/' || path_to_resolve[mount_len] == '\0') {
                // Check if this is the longest match so far
                if (mount_len > best_match_len) {
                    best_match = entry;
                    best_match_len = mount_len;
                }
            }
        }
        
        entry = entry->next;
    }
    
    // If no mount point matches, use the current directory
    if (best_match == NULL) {
        *real_path = dm_malloc(ctx, strlen(path_to_resolve) + 1);
        if (*real_path == NULL) {
            if (abs_path != NULL) {
                dm_free(ctx, abs_path);
            }
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        // Just copy the path as-is
        strcpy(*real_path, path_to_resolve);
    } else {
        // Replace mount point with real path
        const char *rel_path = path_to_resolve + best_match_len;
        
        // Skip leading slash in rel_path
        if (*rel_path == '/') {
            rel_path++;
        }
        
        // Allocate real path
        *real_path = dm_malloc(ctx, strlen(best_match->real_path) + strlen(rel_path) + 2);
        if (*real_path == NULL) {
            if (abs_path != NULL) {
                dm_free(ctx, abs_path);
            }
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        // Construct real path
        strcpy(*real_path, best_match->real_path);
        
        // Add separator if needed
        if (strlen(best_match->real_path) > 0 && 
            best_match->real_path[strlen(best_match->real_path) - 1] != '/' &&
            best_match->real_path[strlen(best_match->real_path) - 1] != '\\' &&
            *rel_path != '\0') {
            
            size_t len = strlen(*real_path);
            (*real_path)[len] = vfs->path_separator;
            (*real_path)[len + 1] = '\0';
        }
        
        // Append relative path
        strcat(*real_path, rel_path);
    }
    
    // Free absolute path if allocated
    if (abs_path != NULL) {
        dm_free(ctx, abs_path);
    }
    
    return DM_SUCCESS;
}

// Get working directory
dm_error_t dm_vfs_get_working_dir(dm_context_t *ctx, char **working_dir) {
    if (ctx == NULL || working_dir == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Get VFS
    dm_vfs_t *vfs = get_vfs(ctx);
    if (vfs == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Copy working directory
    *working_dir = dm_malloc(ctx, strlen(vfs->working_dir) + 1);
    if (*working_dir == NULL) {
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    strcpy(*working_dir, vfs->working_dir);
    
    return DM_SUCCESS;
}

// Set working directory
dm_error_t dm_vfs_set_working_dir(dm_context_t *ctx, const char *path) {
    if (ctx == NULL || path == NULL) {
        fprintf(stderr, "dm_vfs_set_working_dir: Invalid argument\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    fprintf(stderr, "dm_vfs_set_working_dir: path=%s\n", path);
    
    // Get VFS
    dm_vfs_t *vfs = get_vfs(ctx);
    if (vfs == NULL) {
        fprintf(stderr, "dm_vfs_set_working_dir: VFS is NULL\n");
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Special cases: "." and ".."
    if (strcmp(path, ".") == 0) {
        // Current directory, no change needed
        return DM_SUCCESS;
    } else if (strcmp(path, "..") == 0) {
        // Parent directory
        char *dir_copy = dm_strdup(ctx, vfs->working_dir);
        if (dir_copy == NULL) {
            fprintf(stderr, "dm_vfs_set_working_dir: Failed to duplicate working dir\n");
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        char *parent = dirname(dir_copy); // Use dirname to get parent
        
        // Allocate new working directory path
        char *new_dir = dm_strdup(ctx, parent);
        if (new_dir == NULL) {
            fprintf(stderr, "dm_vfs_set_working_dir: Failed to duplicate parent dir\n");
            dm_free(ctx, dir_copy);
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        // Free old working directory
        if (vfs->working_dir != NULL) {
            dm_free(ctx, vfs->working_dir);
        }
        
        // Set new working directory
        vfs->working_dir = new_dir;
        
        // Free temp copy
        dm_free(ctx, dir_copy);
        
        fprintf(stderr, "dm_vfs_set_working_dir: Changed to parent: %s\n", new_dir);
        return DM_SUCCESS;
    }
    
    // Regular path
    fprintf(stderr, "dm_vfs_set_working_dir: Getting absolute path for: %s\n", path);
    
    // Resolve path to absolute path
    char *abs_path = NULL;
    dm_error_t err = dm_path_absolute(ctx, path, &abs_path);
    if (err != DM_SUCCESS) {
        fprintf(stderr, "dm_vfs_set_working_dir: Failed to get absolute path: %s\n", 
                dm_error_string(err));
        return err;
    }
    
    fprintf(stderr, "dm_vfs_set_working_dir: Absolute path: %s\n", abs_path);
    
    // Check if directory exists
    bool exists = false;
    err = dm_dir_exists(ctx, abs_path, &exists);
    if (err != DM_SUCCESS) {
        fprintf(stderr, "dm_vfs_set_working_dir: Failed to check if directory exists: %s\n", 
                dm_error_string(err));
        dm_free(ctx, abs_path);
        return err;
    }
    
    if (!exists) {
        fprintf(stderr, "dm_vfs_set_working_dir: Directory does not exist\n");
        dm_free(ctx, abs_path);
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    fprintf(stderr, "dm_vfs_set_working_dir: Directory exists, changing working directory\n");
    
    // Free old working directory
    if (vfs->working_dir != NULL) {
        dm_free(ctx, vfs->working_dir);
    }
    
    // Set new working directory
    vfs->working_dir = abs_path;
    
    fprintf(stderr, "dm_vfs_set_working_dir: New working directory: %s\n", abs_path);
    return DM_SUCCESS;
} 