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

// Store VFS in context
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

// Initialize filesystem
dm_error_t dm_fs_init(dm_context_t *ctx) {
    if (ctx == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Initialize VFS
    return dm_vfs_init(ctx);
}

// Clean up filesystem
void dm_fs_cleanup(dm_context_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Clean up VFS
    dm_vfs_cleanup(ctx);
}

// Convert file mode to stdio mode string
static const char* file_mode_to_str(dm_file_mode_t mode) {
    if (mode & DM_FILE_READ) {
        if (mode & DM_FILE_WRITE) {
            if (mode & DM_FILE_CREATE) {
                if (mode & DM_FILE_TRUNCATE) {
                    return "w+";
                } else {
                    return "r+";
                }
            } else if (mode & DM_FILE_APPEND) {
                return "a+";
            } else {
                return "r+";
            }
        } else {
            return "r";
        }
    } else if (mode & DM_FILE_WRITE) {
        if (mode & DM_FILE_APPEND) {
            return "a";
        } else if (mode & DM_FILE_TRUNCATE) {
            return "w";
        } else {
            return "w";
        }
    }
    
    // Default
    return "r";
}

// Open a file
dm_error_t dm_file_open(dm_context_t *ctx, const char *path, dm_file_mode_t mode, dm_file_t **file) {
    if (ctx == NULL || path == NULL || file == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Allocate file structure
    *file = dm_malloc(ctx, sizeof(dm_file_t));
    if (*file == NULL) {
        dm_free(ctx, real_path);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize file structure
    (*file)->path = real_path;
    (*file)->mode = mode;
    
    // Open file
    const char *mode_str = file_mode_to_str(mode);
    (*file)->handle = fopen(real_path, mode_str);
    if ((*file)->handle == NULL) {
        dm_free(ctx, real_path);
        dm_free(ctx, *file);
        *file = NULL;
        return DM_ERROR_FILE_IO;
    }
    
    return DM_SUCCESS;
}

// Close a file
dm_error_t dm_file_close(dm_context_t *ctx, dm_file_t *file) {
    if (ctx == NULL || file == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Close file
    if (file->handle != NULL) {
        fclose(file->handle);
        file->handle = NULL;
    }
    
    // Free path
    if (file->path != NULL) {
        dm_free(ctx, file->path);
        file->path = NULL;
    }
    
    // Free file structure
    dm_free(ctx, file);
    
    return DM_SUCCESS;
}

// Read from a file
dm_error_t dm_file_read(dm_context_t *ctx, dm_file_t *file, void *buffer, size_t size, size_t *bytes_read) {
    if (ctx == NULL || file == NULL || buffer == NULL || bytes_read == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    if (file->handle == NULL) {
        return DM_ERROR_FILE_IO;
    }
    
    // Read from file
    *bytes_read = fread(buffer, 1, size, file->handle);
    
    // Check for errors
    if (*bytes_read < size && !feof(file->handle)) {
        return DM_ERROR_FILE_IO;
    }
    
    return DM_SUCCESS;
}

// Write to a file
dm_error_t dm_file_write(dm_context_t *ctx, dm_file_t *file, const void *buffer, size_t size, size_t *bytes_written) {
    if (ctx == NULL || file == NULL || buffer == NULL || bytes_written == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    if (file->handle == NULL) {
        return DM_ERROR_FILE_IO;
    }
    
    // Write to file
    *bytes_written = fwrite(buffer, 1, size, file->handle);
    
    // Check for errors
    if (*bytes_written < size) {
        return DM_ERROR_FILE_IO;
    }
    
    return DM_SUCCESS;
}

// Seek within a file
dm_error_t dm_file_seek(dm_context_t *ctx, dm_file_t *file, long offset, int whence) {
    if (ctx == NULL || file == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    if (file->handle == NULL) {
        return DM_ERROR_FILE_IO;
    }
    
    // Seek within file
    if (fseek(file->handle, offset, whence) != 0) {
        return DM_ERROR_FILE_IO;
    }
    
    return DM_SUCCESS;
}

// Get current position in a file
dm_error_t dm_file_tell(dm_context_t *ctx, dm_file_t *file, long *position) {
    if (ctx == NULL || file == NULL || position == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    if (file->handle == NULL) {
        return DM_ERROR_FILE_IO;
    }
    
    // Get current position
    *position = ftell(file->handle);
    if (*position < 0) {
        return DM_ERROR_FILE_IO;
    }
    
    return DM_SUCCESS;
}

// Check if at end of file
dm_error_t dm_file_eof(dm_context_t *ctx, dm_file_t *file, bool *eof) {
    if (ctx == NULL || file == NULL || eof == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    if (file->handle == NULL) {
        return DM_ERROR_FILE_IO;
    }
    
    // Check if at end of file
    *eof = feof(file->handle) != 0;
    
    return DM_SUCCESS;
}

// Flush file buffers
dm_error_t dm_file_flush(dm_context_t *ctx, dm_file_t *file) {
    if (ctx == NULL || file == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    if (file->handle == NULL) {
        return DM_ERROR_FILE_IO;
    }
    
    // Flush file buffers
    if (fflush(file->handle) != 0) {
        return DM_ERROR_FILE_IO;
    }
    
    return DM_SUCCESS;
}

// Check if a file exists
dm_error_t dm_file_exists(dm_context_t *ctx, const char *path, bool *exists) {
    if (ctx == NULL || path == NULL || exists == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Check if file exists
    struct stat st;
    *exists = (stat(real_path, &st) == 0);
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}

// Get file size
dm_error_t dm_file_size(dm_context_t *ctx, const char *path, size_t *size) {
    if (ctx == NULL || path == NULL || size == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Get file size
    struct stat st;
    if (stat(real_path, &st) != 0) {
        dm_free(ctx, real_path);
        return DM_ERROR_FILE_IO;
    }
    
    *size = (size_t)st.st_size;
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}

// Delete a file
dm_error_t dm_file_delete(dm_context_t *ctx, const char *path) {
    if (ctx == NULL || path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Delete file
    if (unlink(real_path) != 0) {
        dm_free(ctx, real_path);
        return DM_ERROR_FILE_IO;
    }
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}

// Rename a file
dm_error_t dm_file_rename(dm_context_t *ctx, const char *old_path, const char *new_path) {
    if (ctx == NULL || old_path == NULL || new_path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual paths to real paths
    char *real_old_path = NULL;
    char *real_new_path = NULL;
    dm_error_t err;
    
    err = dm_vfs_resolve_path(ctx, old_path, &real_old_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    err = dm_vfs_resolve_path(ctx, new_path, &real_new_path);
    if (err != DM_SUCCESS) {
        dm_free(ctx, real_old_path);
        return err;
    }
    
    // Rename file
    if (rename(real_old_path, real_new_path) != 0) {
        dm_free(ctx, real_old_path);
        dm_free(ctx, real_new_path);
        return DM_ERROR_FILE_IO;
    }
    
    // Free real paths
    dm_free(ctx, real_old_path);
    dm_free(ctx, real_new_path);
    
    return DM_SUCCESS;
}

// Copy a file
dm_error_t dm_file_copy(dm_context_t *ctx, const char *src_path, const char *dst_path) {
    if (ctx == NULL || src_path == NULL || dst_path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Open source file
    dm_file_t *src_file = NULL;
    dm_error_t err = dm_file_open(ctx, src_path, DM_FILE_READ, &src_file);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Open destination file
    dm_file_t *dst_file = NULL;
    err = dm_file_open(ctx, dst_path, DM_FILE_WRITE | DM_FILE_CREATE | DM_FILE_TRUNCATE, &dst_file);
    if (err != DM_SUCCESS) {
        dm_file_close(ctx, src_file);
        return err;
    }
    
    // Copy data
    char buffer[4096];
    size_t bytes_read, bytes_written;
    
    while (true) {
        // Read from source
        err = dm_file_read(ctx, src_file, buffer, sizeof(buffer), &bytes_read);
        if (err != DM_SUCCESS) {
            dm_file_close(ctx, src_file);
            dm_file_close(ctx, dst_file);
            return err;
        }
        
        // Check if done
        if (bytes_read == 0) {
            break;
        }
        
        // Write to destination
        err = dm_file_write(ctx, dst_file, buffer, bytes_read, &bytes_written);
        if (err != DM_SUCCESS) {
            dm_file_close(ctx, src_file);
            dm_file_close(ctx, dst_file);
            return err;
        }
        
        // Check if all bytes were written
        if (bytes_written != bytes_read) {
            dm_file_close(ctx, src_file);
            dm_file_close(ctx, dst_file);
            return DM_ERROR_FILE_IO;
        }
    }
    
    // Close files
    dm_file_close(ctx, src_file);
    dm_file_close(ctx, dst_file);
    
    return DM_SUCCESS;
}

// Get file type
dm_error_t dm_file_type(dm_context_t *ctx, const char *path, dm_file_type_t *type) {
    if (ctx == NULL || path == NULL || type == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Get file type
    struct stat st;
    if (stat(real_path, &st) != 0) {
        dm_free(ctx, real_path);
        *type = DM_FILE_TYPE_UNKNOWN;
        return DM_ERROR_FILE_IO;
    }
    
    // Determine file type
    if (S_ISREG(st.st_mode)) {
        *type = DM_FILE_TYPE_REGULAR;
    } else if (S_ISDIR(st.st_mode)) {
        *type = DM_FILE_TYPE_DIRECTORY;
    } else {
        *type = DM_FILE_TYPE_SPECIAL;
    }
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}

// Open a directory
dm_error_t dm_dir_open(dm_context_t *ctx, const char *path, dm_dir_t **dir) {
    if (ctx == NULL || path == NULL || dir == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Allocate directory structure
    *dir = dm_malloc(ctx, sizeof(dm_dir_t));
    if (*dir == NULL) {
        dm_free(ctx, real_path);
        return DM_ERROR_MEMORY_ALLOCATION;
    }
    
    // Initialize directory structure
    (*dir)->path = real_path;
    (*dir)->entries = NULL;
    (*dir)->count = 0;
    
    // Open directory
    DIR *handle = opendir(real_path);
    if (handle == NULL) {
        dm_free(ctx, real_path);
        dm_free(ctx, *dir);
        *dir = NULL;
        return DM_ERROR_FILE_IO;
    }
    
    // Store handle
    (*dir)->handle = handle;
    
    // Read all entries (simpler to read all at once)
    struct dirent *entry;
    dm_dir_entry_t *last_entry = NULL;
    
    while ((entry = readdir(handle)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Create new entry
        dm_dir_entry_t *new_entry = dm_malloc(ctx, sizeof(dm_dir_entry_t));
        if (new_entry == NULL) {
            // Cleanup on error
            dm_dir_close(ctx, *dir);
            *dir = NULL;
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        
        // Copy name
        new_entry->name = dm_malloc(ctx, strlen(entry->d_name) + 1);
        if (new_entry->name == NULL) {
            dm_free(ctx, new_entry);
            dm_dir_close(ctx, *dir);
            *dir = NULL;
            return DM_ERROR_MEMORY_ALLOCATION;
        }
        strcpy(new_entry->name, entry->d_name);
        
        // Get entry type and size
        char *full_path = NULL;
        err = dm_path_join(ctx, real_path, entry->d_name, &full_path);
        if (err != DM_SUCCESS) {
            dm_free(ctx, new_entry->name);
            dm_free(ctx, new_entry);
            dm_dir_close(ctx, *dir);
            *dir = NULL;
            return err;
        }
        
        struct stat st;
        if (stat(full_path, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                new_entry->type = DM_FILE_TYPE_REGULAR;
                new_entry->size = (size_t)st.st_size;
            } else if (S_ISDIR(st.st_mode)) {
                new_entry->type = DM_FILE_TYPE_DIRECTORY;
                new_entry->size = 0;
            } else {
                new_entry->type = DM_FILE_TYPE_SPECIAL;
                new_entry->size = 0;
            }
        } else {
            new_entry->type = DM_FILE_TYPE_UNKNOWN;
            new_entry->size = 0;
        }
        
        dm_free(ctx, full_path);
        
        // Add to linked list
        new_entry->next = NULL;
        if (last_entry == NULL) {
            (*dir)->entries = new_entry;
        } else {
            last_entry->next = new_entry;
        }
        last_entry = new_entry;
        
        // Increment count
        (*dir)->count++;
    }
    
    return DM_SUCCESS;
}

// Close a directory
dm_error_t dm_dir_close(dm_context_t *ctx, dm_dir_t *dir) {
    if (ctx == NULL || dir == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Close directory
    if (dir->handle != NULL) {
        closedir((DIR *)dir->handle);
        dir->handle = NULL;
    }
    
    // Free entries
    dm_dir_entry_t *entry = dir->entries;
    while (entry != NULL) {
        dm_dir_entry_t *next = entry->next;
        
        // Free name
        if (entry->name != NULL) {
            dm_free(ctx, entry->name);
        }
        
        // Free entry
        dm_free(ctx, entry);
        
        entry = next;
    }
    
    // Free path
    if (dir->path != NULL) {
        dm_free(ctx, dir->path);
    }
    
    // Free directory structure
    dm_free(ctx, dir);
    
    return DM_SUCCESS;
}

// Read a directory entry
dm_error_t dm_dir_read(dm_context_t *ctx, dm_dir_t *dir, dm_dir_entry_t **entry) {
    if (ctx == NULL || dir == NULL || entry == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // We pre-read all entries, so this is just an iterator
    static dm_dir_entry_t *current_entry = NULL;
    
    // Initialize current_entry if first call
    if (current_entry == NULL) {
        current_entry = dir->entries;
    } else {
        current_entry = current_entry->next;
    }
    
    // Check if no more entries
    if (current_entry == NULL) {
        *entry = NULL;
        return DM_SUCCESS;
    }
    
    // Return current entry
    *entry = current_entry;
    
    return DM_SUCCESS;
}

// Rewind directory
dm_error_t dm_dir_rewind(dm_context_t *ctx, dm_dir_t *dir) {
    if (ctx == NULL || dir == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // We pre-read all entries, so this just resets the iterator
    rewinddir((DIR *)dir->handle);
    
    return DM_SUCCESS;
}

// Create a directory
dm_error_t dm_dir_create(dm_context_t *ctx, const char *path) {
    if (ctx == NULL || path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Create directory
    if (mkdir(real_path, 0755) != 0) {
        dm_free(ctx, real_path);
        return DM_ERROR_FILE_IO;
    }
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}

// Delete a directory
dm_error_t dm_dir_delete(dm_context_t *ctx, const char *path) {
    if (ctx == NULL || path == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Delete directory
    if (rmdir(real_path) != 0) {
        dm_free(ctx, real_path);
        return DM_ERROR_FILE_IO;
    }
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}

// Check if a directory exists
dm_error_t dm_dir_exists(dm_context_t *ctx, const char *path, bool *exists) {
    if (ctx == NULL || path == NULL || exists == NULL) {
        return DM_ERROR_INVALID_ARGUMENT;
    }
    
    // Resolve virtual path to real path
    char *real_path = NULL;
    dm_error_t err = dm_vfs_resolve_path(ctx, path, &real_path);
    if (err != DM_SUCCESS) {
        return err;
    }
    
    // Check if directory exists
    struct stat st;
    if (stat(real_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        *exists = true;
    } else {
        *exists = false;
    }
    
    // Free real path
    dm_free(ctx, real_path);
    
    return DM_SUCCESS;
}