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
  src/base.c src/text.c src/busybox_text.c src/editors.c
cc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
  '-DEDL_APPID="TOYBX"' -Itests/stubs -Iinclude -Isrc \
  src/host.c src/host_fs.c src/names.c
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=base_edl_main -Dshell_commands=base_commands -c src/base.c -o /tmp/toybox-base.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=text_edl_main -Dshell_commands=text_commands -c src/text.c -o /tmp/toybox-text.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -c src/busybox_text.c -o /tmp/toybox-busybox.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  -Dedl_main=editor_edl_main -Dshell_commands=editor_commands -c src/editors.c -o /tmp/toybox-editors.o
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_modules.c /tmp/toybox-base.o /tmp/toybox-text.o /tmp/toybox-busybox.o /tmp/toybox-editors.o \
  -o /tmp/toybox-module-tests
/tmp/toybox-module-tests
python3 tools/check_sources.py
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_busybox.c src/busybox_text.c -o /tmp/toybox-busybox-tests
/tmp/toybox-busybox-tests
cc -std=c11 -Wall -Wextra -Werror -Itests/stubs -Iinclude -Isrc \
  tests/check_fs.c src/host_fs.c src/names.c -o /tmp/toybox-nativefs-tests
/tmp/toybox-nativefs-tests
