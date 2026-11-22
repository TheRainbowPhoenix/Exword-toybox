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
    shell_history_push(&h,"ls");
    assert(shell_history_count(&h)==3); /* Consecutive duplicate. */
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
    /* Full command lines fit; the oldest are dropped on pool pressure. */
    shell_history_init(&h);
    char longcmd[191];
    memset(longcmd,'x',sizeof longcmd-1);
    longcmd[sizeof longcmd-1]=0;
    for(int i=0;i<4;i++){
        longcmd[0]=(char)('A'+i);
        shell_history_push(&h,longcmd);
    }
    assert(shell_history_count(&h)==2);
    assert(shell_history_get(&h,0)[0]=='C');
    assert(shell_history_get(&h,1)[0]=='D');
    assert(strlen(shell_history_get(&h,1))==190);
    puts("PASS: compact history back/forward, draft restoration, oldest eviction");
}
