#include <stddef.h>
#include "shell_posix_compat.h"
#include <assert.h>
#include <stdio.h>
/* Prove actual POSIX spelling rewrites to EDL port functions. */
int main(void)
{
    assert(O_CREAT==SP_O_CREAT && SEEK_END==2);
    assert(S_ISREG(SP_S_IFREG));
    assert(S_ISDIR(SP_S_IFDIR));
    struct stat st={.st_size=10,.st_mode=SP_S_IFREG};
    assert(st.st_size==10 && S_ISREG(st.st_mode));
    struct termios t={.c_lflag=ICANON|ECHO};
    assert(t.c_lflag==3);
    puts("PASS: opt-in POSIX source compatibility aliases");
}
