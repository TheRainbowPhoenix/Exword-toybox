#include "shell_names.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    assert(shell_name_valid("readme.txt", 0));
    assert(shell_name_valid("A_B-01.C", 0));
    assert(shell_name_valid("123456789012", 1));
    assert(!shell_name_valid("123456789", 0));
    assert(!shell_name_valid("1234567890123", 1));
    assert(!shell_name_valid("a.toolong", 0));
    assert(!shell_name_valid("a.b.c", 0));
    assert(!shell_name_valid("../file", 0));
    assert(!shell_name_valid("", 0));
    assert(!shell_name_valid("bad name", 1));
    assert(shell_path_valid("\\\\drv0\\CASIO\\TOYBX\\_CONTENT\\notes.txt", 0));
    assert(shell_path_valid("\\\\crd0\\SHORT\\123456789012", 1));
    assert(!shell_path_valid("\\\\drv0\\LONGDIRECTORY\\ok.txt", 0));
    assert(!shell_path_valid("\\\\drv0\\foo\\longfilename.txt", 0));
    assert(!shell_path_valid("\\\\drv0\\foo\\test.txt\\sub", 0));
    assert(!shell_path_valid("\\\\drv0", 1));
    assert(!shell_path_valid("\\\\drv0\\folder\\", 1));
    assert(!shell_path_valid("\\\\drv0\\folder\\..\\file.txt", 0));
    puts("PASS: strict ASCII 8.3 filenames / 12-byte directories");
}
