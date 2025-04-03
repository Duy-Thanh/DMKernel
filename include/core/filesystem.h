#ifndef DM_FILESYSTEM_H
#define DM_FILESYSTEM_H

#include "../dmkernel.h"

// File access modes
typedef enum {
    DM_FILE_READ = 1,       // Read only
    DM_FILE_WRITE = 2,      // Write only
    DM_FILE_READWRITE = 3,  // Read and write
    DM_FILE_APPEND = 4,     // Append (with create)
    DM_FILE_CREATE = 8,     // Create new file (with write)
    DM_FILE_TRUNCATE = 16   // Truncate existing file (with write)
} dm_file_mode_t;

// File type
typedef enum {
    DM_FILE_TYPE_REGULAR,   // Regular file
    DM_FILE_TYPE_DIRECTORY, // Directory
    DM_FILE_TYPE_SPECIAL,   // Special file (socket, device, etc.)
    DM_FILE_TYPE_UNKNOWN    // Unknown or error
} dm_file_type_t;

// Directory entry structure
typedef struct dm_dir_entry {
    char *name;
    dm_file_type_t type;
    size_t size;
    struct dm_dir_entry *next;
} dm_dir_entry_t;

// Directory structure
typedef struct dm_dir {
    char *path;
    dm_dir_entry_t *entries;
    size_t count;
    void *handle;          // Internal handle for OS-specific operations
} dm_dir_t;

// File structure
typedef struct dm_file {
    char *path;
    dm_file_mode_t mode;
    FILE *handle;          // Standard C file handle
} dm_file_t;

// Virtual filesystem entry
typedef struct dm_vfs_entry {
    char *name;            // Mount point name
    char *real_path;       // Real filesystem path
    struct dm_vfs_entry *next;
} dm_vfs_entry_t;

// Virtual filesystem
typedef struct dm_vfs {
    dm_vfs_entry_t *mounts;
    char *working_dir;     // Current working directory
    char path_separator;   // Path separator character ('/' or '\\')
} dm_vfs_t;

// Filesystem initialization
dm_error_t dm_fs_init(dm_context_t *ctx);
void dm_fs_cleanup(dm_context_t *ctx);

// File operations
dm_error_t dm_file_open(dm_context_t *ctx, const char *path, dm_file_mode_t mode, dm_file_t **file);
dm_error_t dm_file_close(dm_context_t *ctx, dm_file_t *file);
dm_error_t dm_file_read(dm_context_t *ctx, dm_file_t *file, void *buffer, size_t size, size_t *bytes_read);
dm_error_t dm_file_write(dm_context_t *ctx, dm_file_t *file, const void *buffer, size_t size, size_t *bytes_written);
dm_error_t dm_file_seek(dm_context_t *ctx, dm_file_t *file, long offset, int whence);
dm_error_t dm_file_tell(dm_context_t *ctx, dm_file_t *file, long *position);
dm_error_t dm_file_eof(dm_context_t *ctx, dm_file_t *file, bool *eof);
dm_error_t dm_file_flush(dm_context_t *ctx, dm_file_t *file);
dm_error_t dm_file_exists(dm_context_t *ctx, const char *path, bool *exists);
dm_error_t dm_file_size(dm_context_t *ctx, const char *path, size_t *size);
dm_error_t dm_file_delete(dm_context_t *ctx, const char *path);
dm_error_t dm_file_rename(dm_context_t *ctx, const char *old_path, const char *new_path);
dm_error_t dm_file_copy(dm_context_t *ctx, const char *src_path, const char *dst_path);
dm_error_t dm_file_type(dm_context_t *ctx, const char *path, dm_file_type_t *type);

// Directory operations
dm_error_t dm_dir_open(dm_context_t *ctx, const char *path, dm_dir_t **dir);
dm_error_t dm_dir_close(dm_context_t *ctx, dm_dir_t *dir);
dm_error_t dm_dir_read(dm_context_t *ctx, dm_dir_t *dir, dm_dir_entry_t **entry);
dm_error_t dm_dir_rewind(dm_context_t *ctx, dm_dir_t *dir);
dm_error_t dm_dir_create(dm_context_t *ctx, const char *path);
dm_error_t dm_dir_delete(dm_context_t *ctx, const char *path);
dm_error_t dm_dir_exists(dm_context_t *ctx, const char *path, bool *exists);

// Path operations
dm_error_t dm_path_join(dm_context_t *ctx, const char *path1, const char *path2, char **result);
dm_error_t dm_path_absolute(dm_context_t *ctx, const char *path, char **abs_path);
dm_error_t dm_path_basename(dm_context_t *ctx, const char *path, char **basename);
dm_error_t dm_path_dirname(dm_context_t *ctx, const char *path, char **dirname);
dm_error_t dm_path_extension(dm_context_t *ctx, const char *path, char **extension);

// Virtual filesystem operations
dm_error_t dm_vfs_init(dm_context_t *ctx);
dm_error_t dm_vfs_cleanup(dm_context_t *ctx);
dm_error_t dm_vfs_mount(dm_context_t *ctx, const char *mount_point, const char *real_path);
dm_error_t dm_vfs_unmount(dm_context_t *ctx, const char *mount_point);
dm_error_t dm_vfs_resolve_path(dm_context_t *ctx, const char *virtual_path, char **real_path);
dm_error_t dm_vfs_get_working_dir(dm_context_t *ctx, char **working_dir);
dm_error_t dm_vfs_set_working_dir(dm_context_t *ctx, const char *path);

#endif /* DM_FILESYSTEM_H */ 