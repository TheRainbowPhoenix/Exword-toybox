/* Extra EX-word adaptations of BusyBox-style file-oriented applet semantics.
   These 18 implementations are newly written for the EDL API; they are NOT
   linked upstream BusyBox code. Existing BusyBox-derived ports keep their
   original GPL notice in busybox_text.c. */
#include "utils_util.h"
#include "shell_format.h"
__attribute__((used,section(".edl.cmd"),aligned(4)))
const struct shell_manifest shell_commands={
    .magic=SHELL_MANIFEST_MAGIC,.version=SHELL_MANIFEST_VERSION,.count=18,
    .names={"rev","tac","nl","cut","tr","strings","comm","paste","fold","expand",
            "seq","cmp","cksum","stat","du","find","printf","false"}
};
int utils_text_main(const struct shell_api *sh);
int utils_misc_main(const struct shell_api *sh);
int edl_main(const struct edl_api *edl)
{
    const struct shell_api *sh=shell_from_edl(edl);
    if(!sh || !sh->command || sh->argc<1)return -1;
    if(m_equal(sh->command,"false"))return sh->argc==1?1:
                       m_usage(sh,"usage: false");
    if(m_equal(sh->command,"rev")||m_equal(sh->command,"tac")||
       m_equal(sh->command,"nl")||m_equal(sh->command,"cut")||
       m_equal(sh->command,"tr")||m_equal(sh->command,"strings")||
       m_equal(sh->command,"comm")||m_equal(sh->command,"paste")||
       m_equal(sh->command,"fold")||m_equal(sh->command,"expand"))
        return utils_text_main(sh);
    return utils_misc_main(sh);
}
