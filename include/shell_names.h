#ifndef TOYBOX_SHELL_NAMES_H
#define TOYBOX_SHELL_NAMES_H
/* Conservative ASCII 8.3 file and 12-byte directory naming policy.
   Invalid names are REJECTED, never silently truncated into a collision. */
#define SHELL_DIR_NAME_MAX 12u
#define SHELL_FILE_STEM_MAX 8u
#define SHELL_FILE_EXT_MAX 3u
int shell_name_valid(const char *name, int directory);
/* Absolute native paths only: \\drv0\PATH\FILE.EXT (also crd0/crd1).
   Only the final component may be a file; parent components are directories. */
int shell_path_valid(const char *path, int directory);
#endif
