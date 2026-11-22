# EX-word Toybox shell — initial implementation

A native **EXWord terminal shell** based on [Edelweiss](https://github.com/TheRainbowPhoenix/Edelweiss/tree/edelweiss).

## Files on the device

```text
TOYBX/_CONTENT/
    toybox.d01      native resident kernel + text terminal + command lookup
    base.edl        ls cat echo cp mv rm touch mkdir date true
    editors.edl     editable vi-lite and nano-lite modes (32 KiB max file)
    text.edl        wc head grep
```

A module declares an applet manifest as a **non-loaded** `.edl.cmd` ELF section,
with a fixed `CMD1` header and 24 fixed 16-byte command names. The D01 scans
adjacent EDLs, validates manifests without executing them, maps names to modules,
then loads exactly one EDL per invocation with the Edelweiss loader. Applets
return to the same D01 and persistent shell state. No recursive EDL loading.

## Controls

Use the touch keyboard under the 528×320 terminal: the first 3 rows enter text,
SHIFT toggles capitals, 123 toggles symbols, SPACE/ESC/DEL/ENTER are along the
bottom. Physical EXE enters, EXIT backspaces, AC/ON or MENU cancels input;
arrow-up recalls the previous command.

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
by controlled tests of disposable files and then the source-level BusyBox port.
