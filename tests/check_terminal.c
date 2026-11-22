#include "shell_terminal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    shell_term_init();
    shell_term_feed("hello\nworld",11);
    assert(!strcmp(shell_term_line(0,6),"hello"));
    assert(!strcmp(shell_term_line(1,6),"world"));
    shell_term_feed("\x1b[2J\x1b[Habc\x1b[1D!",15);
    assert(!strcmp(shell_term_line(0,6),"ab!"));
    shell_term_feed("\rZ\x1b[K",5);
    assert(!strcmp(shell_term_line(0,6),"Z"));
    shell_term_init();
    for(unsigned i=0;i<20;i++) shell_term_feed("line\n",5);
    assert(!strcmp(shell_term_line(4,6),"line") || !strcmp(shell_term_line(4,6),""));
    shell_term_page(1,6);
    assert(shell_term_back()>0);
    shell_term_page(-1,6);
    assert(shell_term_back()==0);
    puts("PASS: ANSI cursor/clear and bounded console paging");
}
