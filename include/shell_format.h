#ifndef TOYBOX_SHELL_FORMAT_H
#define TOYBOX_SHELL_FORMAT_H
#include <stdint.h>
#include <stddef.h>
#define SHELL_MANIFEST_MAGIC 0x434d4431u /* CMD1 */
#define SHELL_MANIFEST_VERSION 1u
#define SHELL_MAX_COMMANDS 24u
#define SHELL_NAME_SIZE 16u
#define SHELL_MAX_MODULES 4u
#define SHELL_MAX_ARGS 16u
#define SHELL_LINE_SIZE 192u
struct shell_manifest {
    uint32_t magic, version, count;
    char names[SHELL_MAX_COMMANDS][SHELL_NAME_SIZE];
};
typedef int (*shell_read_fn)(void *ctx, uint32_t at, void *dst, size_t bytes);
/* Extracts a non-ALLOC ELF section from an ELF32 big-endian SH executable.
   Returns 0 on a valid supported manifest, -1 for all invalid inputs. */
int shell_manifest_read(shell_read_fn read_fn, void *ctx, uint32_t size,
                        struct shell_manifest *out);
#endif
