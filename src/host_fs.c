#include "host_fs.h"
#include "shell_names.h"
#include <gint/exword.h>
#include <gint/gint.h>
#include <string.h>
#include <stdint.h>

/* World switches are mandatory: native file syscalls must not run directly
   while gint owns the EX-word device context. */
struct size_query { const char *path; int result; };
static int size_native(struct size_query *q)
{
    int fd = exword_sys_open(q->path, 1);
    if (fd < 0) return -1;
    int n = exword_sys_get_filesize(fd);
    int rc = exword_sys_close(fd);
    return rc < 0 ? -1 : n;
}
int host_fs_size(const char *path)
{
    if (!path) return -1;
    struct size_query q = {.path = path};
    return gint_world_switch(GINT_CALL(size_native, (void *)&q));
}
static int same_name(const char *a, const char *b)
{
    while (*a && *b) {
        char x = *a++, y = *b++;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return !*a && !*b;
}
static int make_glob(const char *path, char out[EDL_PATH_MAX])
{
    size_t n = strlen(path);
    if (!n || n+3 >= EDL_PATH_MAX) return -1;
    memcpy(out, path, n);
    if (out[n-1] != '\\') out[n++] = '\\';
    out[n++] = '*'; out[n] = 0;
    return 0;
}
static int exists_dir_native(const char *path)
{
    /* Filesystem roots have no parent entry. */
    if (same_name(path, "\\\\drv0") || same_name(path, "\\\\crd0") ||
        same_name(path, "\\\\crd1")) return 1;
    const char *leaf = strrchr(path, '\\');
    if (!leaf || leaf == path || !leaf[1]) return 0;
    char pattern[EDL_PATH_MAX];
    size_t n = (size_t)(leaf - path);
    if (n + 3 >= sizeof pattern) return 0;
    memcpy(pattern, path, n); pattern[n++] = '\\';
    pattern[n++] = '*'; pattern[n] = 0;
    char name[512] = {0}; int fd = -1;
    unsigned long kind = 0;
    int rc = exword_sys_findfirst(pattern, &fd, name, &kind);
    if (rc < 0) return 0;
    int found = 0;
    do {
        name[sizeof name-1] = 0;
        if (kind == 5 && same_name(leaf+1, name)) { found = 1; break; }
        name[0] = 0; kind = 0;
        rc = exword_sys_findnext(fd, name, &kind);
    } while (rc >= 0);
    exword_sys_findclose(fd);
    return found;
}
struct dir_query {
    const char *path;
    unsigned first, cap;
    struct shell_dirent *items;
};
static int list_native(struct dir_query *q)
{
    char pattern[EDL_PATH_MAX], name[512] = {0};
    if (make_glob(q->path, pattern)) return -1;
    int handle = -1;
    unsigned long kind = 0;
    int rc = exword_sys_findfirst(pattern, &handle, name, &kind);
    if (rc < 0) return exists_dir_native(q->path) ? 0 : -1;
    unsigned total = 0;
    do {
        name[sizeof name-1] = 0;
        if (strcmp(name, ".") && strcmp(name, "..")) {
            if (total >= q->first && total-q->first < q->cap) {
                struct shell_dirent *e = &q->items[total-q->first];
                size_t n = strlen(name);
                if (n >= sizeof e->name) n = sizeof e->name - 1;
                memcpy(e->name, name, n); e->name[n] = 0;
                e->directory = kind == 5;
            }
            total++;
        }
        name[0] = 0; kind = 0;
        rc = exword_sys_findnext(handle, name, &kind);
    } while (rc >= 0);
    int close_rc = exword_sys_findclose(handle);
    return close_rc < 0 ? -1 : (int)total;
}
int host_fs_list(const char *path, unsigned first, struct shell_dirent *items, unsigned capacity)
{
    if (!path || (capacity && !items) || capacity > 64 || first > 65535) return -1;
    struct dir_query q = {path, first, capacity, items};
    return gint_world_switch(GINT_CALL(list_native, (void *)&q));
}
static int exists_native(struct size_query *q) { return exists_dir_native(q->path); }
int host_fs_dir_exists(const char *path)
{
    if (!path) return 0;
    struct size_query q = {.path = path};
    return gint_world_switch(GINT_CALL(exists_native, (void *)&q));
}
struct write_query {
    const char *path;
    unsigned offset, bytes, flags;
    const void *data;
};
static int write_native(struct write_query *q)
{
    if (!q->path || (q->bytes && !q->data) || q->offset > 0x7fffffffU ||
        q->bytes > 0x7fffffffU-q->offset) return -1;
    if (q->flags & SHELL_WRITE_TRUNC) {
        /* EX-word has no documented O_TRUNC, so recreate on explicit truncate.
           Not atomic: callers should keep backups of important data. */
        int old = exword_sys_open(q->path, 1);
        if (old >= 0) { exword_sys_close(old); if (exword_sys_delete(q->path) < 0) return -1; }
        if (exword_sys_create(q->path, 1) < 0) return -1;
    }
    int fd = exword_sys_open(q->path, 3);
    if (fd < 0 && (q->flags & SHELL_WRITE_CREATE)) {
        if (exword_sys_create(q->path, 1) < 0) return -1;
        fd = exword_sys_open(q->path, 3);
    }
    if (fd < 0) return -1;
    int rc = -1;
    if (exword_sys_seek(fd, (int)q->offset, 0) >= 0) {
        unsigned at = 0;
        while (at < q->bytes) {
            int n = exword_sys_write(fd, (const unsigned char *)q->data+at,
                                     (int)(q->bytes-at));
            if (n <= 0 || (unsigned)n > q->bytes-at) break;
            at += (unsigned)n;
        }
        rc = at == q->bytes ? (int)at : -1;
    }
    if (exword_sys_close(fd) < 0) return -1;
    return rc;
}
int host_fs_write(const char *path, unsigned offset, const void *data,
                  unsigned length, unsigned flags)
{
    if (!shell_path_valid(path,0) || (flags & ~(SHELL_WRITE_CREATE|SHELL_WRITE_TRUNC)))
        return -1;
    struct write_query q = {path, offset, length, flags, data};
    return gint_world_switch(GINT_CALL(write_native, (void *)&q));
}
struct rename_query { const char *old_path, *new_path; int directory; };
static int delete_native(struct size_query *q) { return exword_sys_delete(q->path); }
int host_fs_delete(const char *path)
{
    struct size_query q = {.path = path};
    return shell_path_valid(path,host_fs_dir_exists(path)) ?
           gint_world_switch(GINT_CALL(delete_native, (void *)&q)) : -1;
}
static int rename_native(struct rename_query *q)
{ return exword_sys_rename(q->old_path, q->new_path); }
int host_fs_rename(const char *old_path, const char *new_path)
{
    int directory=host_fs_dir_exists(old_path);
    if (!shell_path_valid(old_path,directory) ||
        !shell_path_valid(new_path,directory)) return -1;
    struct rename_query q = {.old_path = old_path, .new_path = new_path};
    return gint_world_switch(GINT_CALL(rename_native, (void *)&q));
}
static int create_native(struct rename_query *q)
{
    /* SDK's type=1 is a file; directory type=5 matches EX-word directory entries.
       Creation/type semantics match EXWord-Explorer. Validate names before I/O. */
    return exword_sys_create(q->old_path, q->directory ? 5 : 1);
}
int host_fs_create(const char *path, int directory)
{
    if (!shell_path_valid(path,directory)) return -1;
    struct rename_query q = {.old_path = path, .directory = directory};
    return gint_world_switch(GINT_CALL(create_native, (void *)&q));
}
