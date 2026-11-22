# EX-word Toybox shell — initial implementation

A native **EXWord terminal shell** based on [Edelweiss](https://github.com/TheRainbowPhoenix/Edelweiss/tree/edelweiss).

## Files on the device

```text
TOYBX/_CONTENT/
    toybox.d01      native resident kernel + text terminal + command lookup
    base.edl        ls cat echo cp mv rm touch mkdir rmdir date true
    editors.edl     editable vi-lite and nano-lite modes (32 KiB max file)
    text.edl        wc head grep basename dirname uniq sort tail more hexdump
```

A module declares an applet manifest as a **non-loaded** `.edl.cmd` ELF section,
with a fixed `CMD1` header and 24 fixed 16-byte command names. The D01 scans
adjacent EDLs, validates manifests without executing them, maps names to modules,
then loads exactly one EDL per invocation with the Edelweiss loader. Applets
return to the same D01 and persistent shell state. No recursive EDL loading.

## Controls

**Use the physical EX-word QWERTY keyboard by default.** Hold SHIFT for
capitals; hold ALPHA+Q..P for 1..0 (ALPHA+A makes a dot). Physical EXE is
Enter, DEL is Backspace, EXIT is Escape, arrows move the cursor/history,
and EX-word ZOOM (or the 0 key) is Space, JUMP (or SHIFT+0) is Tab. The
EX-word HISTORY key recalls the previous command. Hardware
punctuation keys provide / , . : ( ) + - * and more.

**AUDIO or LAYOUT button** toggles a small on-screen *symbol-only*
keyboard, hidden by default. It is a fallback for punctuation, Space, Tab,
Escape, Del and Enter; if a particular firmware does not report its volume
rocker, use the EX-word AUDIO/LAYOUT key instead. The previous path/title bar
is gone. **Page Up / Page Down** scroll terminal history at the prompt; output
scrollback is bounded and cannot retain an entire large file. Use `more FILE`
for long files. Physical ESC/EXIT remains vi's normal-mode/quit key; only
DEL backspaces.

Examples:

```text
EX> help
EX> ls
EX> cat notes.txt
EX> vi notes.txt
EX> cp notes.txt backup.txt
EX> nano backup.txt
EX> grep TODO notes.txt
EX> cd ..
EX> pwd
EX> exit
```

**vi-lite** supports `h/j/k/l`, arrows, `i` / `a` / `o` insert, `x` delete,
`0` / `$` line edges, `:w`, `:q`, `:q!`, `:wq`, with ESC returning to normal.
**nano-lite** immediately inserts text; ESC shows save/discard/cancel choices.
These are the initial small editors, **not** upstream BusyBox vi or GNU nano.
A proper BusyBox `vi.c` port will be an explicit later step after implementing
`termios`/TTY compatibility and testing on-device.

## API

`include/shell_api.h` extends the upstream `struct edl_api` **by appending**
`command`, `argc`, `argv`, `cwd`, output, input and native FS callbacks; the
existing ABI v1 prefix stays untouched. EDL applets validate `size`, `SHL1` magic
and version before using the extension. Since one EDL replaces the previous
payload in the shared memory arena, no module pointers survive a return.

The default startup directory is the active add-in's `_CONTENT` folder.
Absolute paths can use `/drv0/...`, `/crd0/...` or `/crd1/...` (the host also
accepts native backslashes), while relative paths use the persistent `cwd`.

## CI build and hardware safety

GitHub Actions has **successfully cross-compiled** the native D01 and all three
EDL modules, verified the D01 is at most 128 KiB, and validated the Japanese
and Chinese installation packages. Download the appropriate `Toybox-ja` or
`Toybox-cn` package from [GitHub Actions](https://github.com/TheRainbowPhoenix/Exword-toybox/actions).

**No hardware execution has been performed.** Edelweiss's firmware
fingerprints and executable RAM assumptions remain experimental. Do not use
an unknown firmware or a device you cannot recover, and back up device data.
See the [read-only device smoke-test checklist](docs/SMOKE_TEST.md) before
trying filesystem modifications. Destructive writes, including editor saves,
are not yet validated on physical hardware.

The bounded resident command registry supports **four EDL bundles and 48 total
applet names**. The next milestone is read-only device verification (boot,
keyboard, `help`, `pwd`, `modules`, `ls`, `cat`, module return), followed
by controlled tests of disposable files and further BusyBox applet ports.

## Native filesystem operations and name limits

Toybox uses the same native EX-word filesystem syscall pattern as
[EXWord-Explorer](https://github.com/TheRainbowPhoenix/EXWord-Explorer):
`exword_sys_create(path,1)` for files, `exword_sys_create(path,5)` for folders,
`exword_sys_open(path,3)` for read/write, and OS-world switching for native
calls. All modifying operations are checked at the resident service boundary
**and** in the low-level native filesystem adapter.

New/modified **file** names must use ASCII **8.3** (`NAME1234.TXT`, maximum
8-byte stem and 3-byte extension). Folder names must be at most **12 ASCII
characters** without a dot. Supported name characters are letters, digits,
underscore, hyphen, dollar sign, and tilde. Invalid names are **rejected**, not
silently truncated to a possibly colliding destination.

`mkdir -p` creates missing parent directories; `rmdir` only removes an empty
directory; `rm` refuses to delete a directory. File writes and editor saves
are backed by the native API, but overwrite/truncate still deletes and
recreates the file and is **not atomic**. Never edit important or sole-copy data
before validating on your exact EX-word firmware. Executable `.edl`/`.d01`
files in Toybox's installed directory remain write-protected.

## BusyBox source-level ports

`src/busybox_text.c` adapts algorithms from **BusyBox 0.60.5** for `basename`,
`dirname`, `uniq`, `sort`, and `tail` to the EDL API; copyright notices are
retained there, and `COPYING.BUSYBOX` supplies the GNU GPL version 2 terms.
These are adaptations, not drop-in unmodified binaries. The source code and
building scripts are included in this repository.

The first port deliberately uses bounded memory and explicit input filenames:
`sort` supports `-nru` and `-o OUTPUT` but is limited to **128 lines of at
most 255 bytes**; `uniq` supports `-cdu` and `[INPUT [OUTPUT]]`; `tail`
supports `-n N` and `-c N`. There is no stdin or pipe support, so commands
like `sort` without a filename are not yet available. `vi` and `nano` remain
the separate lightweight editors, not upstream BusyBox vi or GNU nano.

**This is a partial BusyBox port.** A complete BusyBox userspace would require
more POSIX compatibility, terminal handling and substantial additional applet
porting; commands involving processes, fork/exec, network facilities,
permissions, pipes, or signals are not currently supported.

## Experimental POSIX/TTY compatibility layer

The resident shell now understands a bounded subset of ANSI/ECMA-48 terminal
sequences (cursor motion, carriage return, clear screen/line, and SGR ignored
cleanly). The touch keyboard is optional; terminal rows expand when hidden.

EDL applets can use \`include/shell_posix.h\` + \`src/posix.c\` for **0/1/2 TTY
descriptors** and four additional file descriptors: \`sp_open\`, \`sp_read\`,
\`sp_write\`, \`sp_close\`, \`sp_lseek\`, \`sp_stat\`, \`sp_fstat\`, \`sp_getcwd\`,
\`sp_tcgetattr\`, \`sp_tcsetattr\`, \`sp_winsize\` and \`sp_isatty\`.
Termios-like \`SP_ICANON\` and \`SP_ECHO\` modes support basic canonical line
input and raw bytes, with arrow keys returned as ANSI sequences. This is a
source-level POSIX *portability layer*, not a complete POSIX libc. BusyBox
sources still need targeted adaptation, especially for stdio, dynamic
allocation, signals, pipes, fork, exec, and terminal details.

The \`cat\` applet now reads using the descriptor API, while both file reads
and writes cross the EX-word OS-world boundary through **resident staging
buffers** rather than passing EDL arena buffers to native syscalls. This
addresses one plausible cause of corrupted plain-ASCII reads. Device-side
confirmation is required. \`hexdump FILE\` inspects raw file bytes and
\`more FILE\` pauses between pages so the original text can be compared.

The display font only renders ASCII directly; UTF-8, Shift-JIS, and arbitrary
binary bytes are not yet decoded. ANSI support is partial, and a genuine
BusyBox vi port is still future work; vi-lite/nano-lite remain in editors.edl.

### Porting further BusyBox applets

Opt in to \`include/shell_posix_compat.h\` in a small C applet source to map
common POSIX names such as \`open/read/write/lseek/close/stat/fstat/isatty\`,
\`opendir/readdir/closedir\`, \`rename/unlink/mkdir/rmdir\` and a subset of
\`termios\` to the EDL descriptors. The code is **not ABI-compatible with
glibc/libbb**; upstream BusyBox build files and libbb still need adaptation,
and \`errno\` is not implemented. This is for portable file-oriented applets,
not process- or network-related BusyBox programs.

The resident stores only module basenames instead of four redundant 320-byte
full paths, leaving RAM for the staged I/O and 32-row bounded terminal history.
Left/Right edit the command line; Up recalls the last line, Down clears the
entry, and Tab works as a shell argument separator.

### Volume-rocker key identification

gint-exword exposes the EX-word AUDIO and LAYOUT keycodes but does not name a
separate volume-up/down rocker in \`keycodes.h\`. If the physical volume control
does not toggle the touch-symbol panel, run \`keyscan\`, press the rocker,
note the displayed hexadecimal code (if any), then press EXIT. This helps add
the precise mapping for that device/firmware; unseen keys cannot be mapped.
