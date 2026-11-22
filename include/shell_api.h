#ifndef TOYBOX_SHELL_API_H
#define TOYBOX_SHELL_API_H
#include "shell_format.h"
#include "shell_keys.h"
#include <edl.h>
#define SHELL_SERVICE_MAGIC 0x53484c31u /* SHL1 */
#define SHELL_SERVICE_VERSION 1u
#define SHELL_WRITE_CREATE 1u
#define SHELL_WRITE_TRUNC  2u
struct shell_dirent { char name[128]; int directory; };
/* ABI extension: the original edl_api must be the first member, unchanged.
   Caller owns pointers; EDL modules must not retain them across return. */
struct shell_api {
    struct edl_api edl;
    uint32_t shell_magic, shell_version;
    const char *command;
    int argc;
    const char *const *argv;
    const char *cwd;
    int (*console_write)(const void *bytes, unsigned length);
    int (*read_key)(void);
    int (*resolve_path)(const char *given, char out[EDL_PATH_MAX]);
    int (*file_size)(const char *absolute);
    int (*list_dir)(const char *absolute, unsigned first,
                    struct shell_dirent *items, unsigned capacity);
    int (*file_write)(const char *absolute, unsigned offset, const void *bytes,
                      unsigned length, unsigned flags);
    int (*file_delete)(const char *absolute);
    int (*file_rename)(const char *old_absolute, const char *new_absolute);
    int (*file_create)(const char *absolute, int directory);
    int (*tty_size)(unsigned *cols, unsigned *rows);
};
static inline const struct shell_api *shell_from_edl(const struct edl_api *api)
{
    if (!api || api->size < sizeof(struct shell_api)) return NULL;
    const struct shell_api *sh = (const struct shell_api *)api;
    return sh->shell_magic == SHELL_SERVICE_MAGIC &&
           sh->shell_version == SHELL_SERVICE_VERSION ? sh : NULL;
}
#endif
