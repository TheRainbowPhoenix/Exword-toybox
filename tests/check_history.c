#include "shell_history.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void)
{
    struct shell_history h;
    shell_history_init(&h);
    assert(shell_history_up(&h,"draft")==NULL);
    shell_history_push(&h,"help");
    shell_history_push(&h,"pwd");
    shell_history_push(&h,"ls");
    assert(shell_history_count(&h)==3);
    assert(!strcmp(shell_history_up(&h,"cat notes.txt"),"ls"));
    assert(!strcmp(shell_history_up(&h,"ignored"),"pwd"));
    assert(!strcmp(shell_history_up(&h,"ignored"),"help"));
    assert(!strcmp(shell_history_up(&h,"ignored"),"help"));
    assert(!strcmp(shell_history_down(&h),"pwd"));
    assert(!strcmp(shell_history_down(&h),"ls"));
    assert(!strcmp(shell_history_down(&h),"cat notes.txt"));
    assert(shell_history_down(&h)==NULL);
    shell_history_push(&h,"ls"); /* consecutive duplicate */
    assert(shell_history_count(&h)==3);
    for(int i=0;i<13;i++){
        char buf[20];snprintf(buf,sizeof buf,"cmd%d",i);
        shell_history_push(&h,buf);
    }
    assert(shell_history_count(&h)==SHELL_HISTORY_DEPTH);
    assert(!strcmp(shell_history_get(&h,0),"cmd5"));
    assert(!strcmp(shell_history_get(&h,7),"cmd12"));
    assert(shell_history_get(&h,8)==NULL);
    assert(!strcmp(shell_history_up(&h,""),"cmd12"));
    assert(!strcmp(shell_history_up(&h,""),"cmd11"));
    shell_history_push(&h," ");
    assert(shell_history_count(&h)==8);
    puts("PASS: eight-entry command history, back/forward, draft restoration, wrap");
}
