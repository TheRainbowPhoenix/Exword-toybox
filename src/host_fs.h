#ifndef TOYBOX_HOST_FS_H
#define TOYBOX_HOST_FS_H
#include "shell_api.h"
int host_fs_size(const char *path);
int host_fs_list(const char *path, unsigned first, struct shell_dirent *items, unsigned capacity);
int host_fs_write(const char *path, unsigned offset, const void *data, unsigned bytes, unsigned flags);
int host_fs_delete(const char *path);
int host_fs_rename(const char *old_path, const char *new_path);
int host_fs_create(const char *path, int directory);
int host_fs_dir_exists(const char *path);
#endif
