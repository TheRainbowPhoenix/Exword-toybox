#include "shell_format.h"
#include <string.h>
#include <limits.h>
static uint16_t be16(const unsigned char *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static uint32_t be32(const unsigned char *p) { return ((uint32_t)be16(p) << 16) | be16(p+2); }
static int bounds(uint32_t at, uint32_t n, uint32_t size)
{ return at <= size && n <= size - at; }
static int command_name(const char *s)
{
    unsigned j = 0;
    for (; j < SHELL_NAME_SIZE && s[j]; j++) {
        unsigned char c = (unsigned char)s[j];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return 0;
    }
    return j && j < SHELL_NAME_SIZE;
}
int shell_manifest_read(shell_read_fn read_fn, void *ctx, uint32_t size,
                        struct shell_manifest *out)
{
    unsigned char h[52], sec[40], names[40], name[9], raw[12];
    if (!read_fn || !out || size < sizeof h || read_fn(ctx, 0, h, sizeof h) ||
        memcmp(h, "\177ELF\1\2\1", 7) || be16(h+16) != 2 ||
        be16(h+18) != 42 || be16(h+46) != 40) return -1;
    uint32_t base = be32(h+32);
    unsigned count = be16(h+48), stridx = be16(h+50);
    if (!count || count > 512 || stridx >= count ||
        !bounds(base, count*40u, size) ||
        read_fn(ctx, base+stridx*40u, names, sizeof names)) return -1;
    uint32_t strbase = be32(names+16), strsize = be32(names+20);
    if (be32(names+4) != 3 || !bounds(strbase, strsize, size)) return -1;
    int found = 0;
    for (unsigned i=0; i<count; i++) {
        if (read_fn(ctx, base+i*40u, sec, sizeof sec)) return -1;
        uint32_t noff = be32(sec);
        if (!bounds(noff, sizeof name, strsize) ||
            read_fn(ctx, strbase+noff, name, sizeof name)) continue;
        if (memcmp(name, ".edl.cmd\0", sizeof name)) continue;
        if (found++ || be32(sec+4) != 1 || (be32(sec+8) & 2) ||
            be32(sec+20) != sizeof(struct shell_manifest) ||
            !bounds(be32(sec+16), be32(sec+20), size) ||
            read_fn(ctx, be32(sec+16), raw, sizeof raw)) return -1;
        uint32_t n = be32(raw+8);
        if (be32(raw) != SHELL_MANIFEST_MAGIC ||
            be32(raw+4) != SHELL_MANIFEST_VERSION ||
            n == 0 || n > SHELL_MAX_COMMANDS) return -1;
        struct shell_manifest temp = {0};
        temp.magic = SHELL_MANIFEST_MAGIC;
        temp.version = SHELL_MANIFEST_VERSION;
        temp.count = n;
        if (read_fn(ctx, be32(sec+16)+12, temp.names, sizeof temp.names)) return -1;
        for (unsigned j=0; j<n; j++) {
            if (!command_name(temp.names[j])) return -1;
            for (unsigned k=0; k<j; k++)
                if (!strcmp(temp.names[j], temp.names[k])) return -1;
        }
        *out = temp;
    }
    return found == 1 ? 0 : -1;
}
