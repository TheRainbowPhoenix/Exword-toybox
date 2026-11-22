# Read-only EX-word Toybox smoke test

The CI build validates the SuperH ELF/D01 *format*, compilation, link-time RAM
and file sizes. **It does not establish that executing the code is safe on a
physical EX-word.** Edelweiss firmware fingerprints and its executable arena
lease are still experimental. Do not try unknown firmware or a device that you
cannot recover. Back up all device/SD data before installing or starting.

## Package and setup

1. Confirm the ROM/firmware matches an Edelweiss fingerprinted profile and
   that you have an independent device recovery procedure. Do **not** bypass
   the firmware/arena rejection on an unknown ROM.
2. Obtain the relevant `Toybox-ja` or `Toybox-cn` artifact from the successful
   [GitHub Actions build](https://github.com/TheRainbowPhoenix/Exword-toybox/actions).
   Its `TOYBX` directory contains the add-in metadata and `_CONTENT` files.
3. Verify that `_CONTENT` contains the shell code bundles
   `toybox.d01`, `base.edl`, `editors.edl`, `text.edl` (in addition to
   installer metadata outside `_CONTENT`). Follow the device's existing,
   model-appropriate add-in installation process.
4. Arrange a known, existing **nonessential** text file for `cat` before
   booting; do not make a new file using the shell during initial tests.

## First launch — no write commands

Record the exact model and ROM version, then verify these in order:

- [ ] Boot shows a Toybox terminal and accepts touch keyboard input.
- [ ] `help` lists builtins and commands from the three EDL bundles.
- [ ] `pwd` shows the active package directory.
- [ ] `modules` lists `base.edl`, `editors.edl`, and `text.edl`.
- [ ] `ls` enumerates the package directory correctly.
- [ ] `cat` on the pre-existing nonessential text file prints the expected data.
- [ ] A command from `text.edl`, such as `head <filename>`, returns to the
      same prompt with `pwd` unchanged.
- [ ] `exit` returns to the OS cleanly; launching again still works.

**Do not run** `cp`, `mv`, `rm`, `touch`, `mkdir`, `vi :w`, or a nano save during
this stage. A crash, hang, garbled screen, absent module or unexpected native
filesystem error is a stop condition; save logs/photographs and investigate
before continuing. A passing build alone is not permission to test destructive
writes on important data.

## Later work (not yet validated)

Only after a clean read-only smoke test should controlled write tests use
**disposable** files and an independently verified backup. Check create,
partial writes, truncate/recreate, rename and delete separately; the current
truncate implementation deletes and recreates a file and is not atomic.
Validate editor save/quit on disposable data. Future BusyBox ports need their
own terminal and filesystem compatibility tests, plus GPL compliance.
