#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  src/parser.c src/manifest.c tests/check_host.c -o /tmp/toybox-host-tests
/tmp/toybox-host-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  src/names.c tests/check_names.c -o /tmp/toybox-name-tests
/tmp/toybox-name-tests
cc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
  -Itests/stubs -Iinclude -Isrc \
  src/base.c src/text.c src/busybox_text.c src/editors.c src/posix.c src/posix_tools.c \
  src/utils.c src/utils_io.c src/utils_text.c src/utils_misc.c
cc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
  '-DEDL_APPID="TOYBX"' -Itests/stubs -Iinclude -Isrc \
  src/host.c src/host_fs.c src/names.c src/history.c src/keymap.c src/terminal.c src/io_bridge.c
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=base_edl_main -Dshell_commands=base_commands -c src/base.c -o /tmp/toybox-base.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc -c src/posix.c -o /tmp/toybox-posix.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc -c src/posix_tools.c -o /tmp/toybox-tools.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=text_edl_main -Dshell_commands=text_commands -c src/text.c -o /tmp/toybox-text.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -c src/busybox_text.c -o /tmp/toybox-busybox.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=editor_edl_main -Dshell_commands=editor_commands -c src/editors.c -o /tmp/toybox-editors.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_modules.c /tmp/toybox-base.o /tmp/toybox-text.o /tmp/toybox-busybox.o /tmp/toybox-tools.o /tmp/toybox-posix.o /tmp/toybox-editors.o \
  -o /tmp/toybox-module-tests
/tmp/toybox-module-tests
python3 tools/check_sources.py
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_busybox.c src/busybox_text.c -o /tmp/toybox-busybox-tests
/tmp/toybox-busybox-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_fs.c src/host_fs.c src/manifest.c src/names.c -o /tmp/toybox-nativefs-tests
/tmp/toybox-nativefs-tests
cc -std=c11 -Wall -Wextra -Werror -Iinclude src/terminal.c tests/check_terminal.c -o /tmp/toybox-terminal-tests
/tmp/toybox-terminal-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude src/keymap.c tests/check_keys.c -o /tmp/toybox-key-tests
/tmp/toybox-key-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude src/posix.c tests/check_posix.c -o /tmp/toybox-posix-tests
/tmp/toybox-posix-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude src/io_bridge.c tests/check_io.c -o /tmp/toybox-io-tests
/tmp/toybox-io-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude \
  tests/check_compat.c -o /tmp/toybox-compat-tests
/tmp/toybox-compat-tests
cc -std=c11 -Wall -Wextra -Werror -pedantic -Iinclude \
  src/history.c tests/check_history.c -o /tmp/toybox-history-tests
/tmp/toybox-history-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=utils_edl_main -Dshell_commands=utils_commands \
  -c src/utils.c -o /tmp/toybox-utils.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -c src/utils_io.c -o /tmp/toybox-utils-io.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -c src/utils_text.c -o /tmp/toybox-utils-text.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -c src/utils_misc.c -o /tmp/toybox-utils-misc.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_utils.c /tmp/toybox-utils.o /tmp/toybox-utils-io.o \
  /tmp/toybox-utils-text.o /tmp/toybox-utils-misc.o \
  -o /tmp/toybox-utils-tests
/tmp/toybox-utils-tests
