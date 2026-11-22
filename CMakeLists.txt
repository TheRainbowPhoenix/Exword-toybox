cmake_minimum_required(VERSION 3.24)
project(ExwordToybox LANGUAGES C ASM)
if(NOT FXSDK_PLATFORM_LONG STREQUAL "exword")
  message(FATAL_ERROR "Configure with the fxSDK EXWORD.cmake toolchain")
endif()

set(EDELWEISS_REF "edelweiss" CACHE STRING "Edelweiss branch/tag/commit")
set(EXWORD_APP_ID "TOYBX" CACHE STRING "5-character EX-word install ID")
set(EXWORD_MODULE "toybox" CACHE STRING "D01 filename, <=6 characters for elf2d01")
set(EXWORD_APP_TITLE "Toybox Shell" CACHE STRING "Dictionary add-in title")
option(SHELL_BUILD_PACKAGES "Build localized install packages" ON)
if(NOT EXWORD_APP_ID MATCHES "^[A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9][A-Z0-9]$")
  message(FATAL_ERROR "EXWORD_APP_ID must be 5 uppercase letters/digits")
endif()
string(LENGTH "${EXWORD_MODULE}" module_len)
if(NOT EXWORD_MODULE MATCHES "^[a-z][a-z0-9_]*$" OR module_len GREATER 6)
  message(FATAL_ERROR "EXWORD_MODULE must be a lowercase 8.3-compatible name")
endif()

include(FetchContent)
set(EDL_BUILD_PAYLOADS OFF CACHE BOOL "Disable upstream example payloads" FORCE)
set(EDL_BUILD_PACKAGES OFF CACHE BOOL "Disable upstream packages" FORCE)
set(EDL_APPID "${EXWORD_APP_ID}" CACHE STRING "Edelweiss bootstrap fallback ID" FORCE)
FetchContent_Declare(edelweiss GIT_REPOSITORY https://github.com/TheRainbowPhoenix/Edelweiss.git
  GIT_TAG "${EDELWEISS_REF}")
FetchContent_MakeAvailable(edelweiss)
include(GenerateD01)
include(GenerateEXWORDPackage)
include("${edelweiss_SOURCE_DIR}/sdk/GenerateEDL.cmake")
find_package(Gint 2.11 REQUIRED)

# Keep .edl.cmd in the non-ALLOC section table, not the executable arena.
# This patch changes only our generated linker script, not upstream sources.
set(EDL_LINKER "${CMAKE_CURRENT_BINARY_DIR}/edl.ld")
file(READ "${EDL_LINKER}" edl_script)
set(marker "    .edl.meta 0 (INFO) : { KEEP(*(.edl.meta)) } :NONE")
string(FIND "${edl_script}" "${marker}" marker_offset)
if(marker_offset EQUAL -1)
  message(FATAL_ERROR "Unsupported Edelweiss SDK linker script: .edl.meta missing")
endif()
string(REPLACE "${marker}" "${marker}\n    .edl.cmd 0 (INFO) : { KEEP(*(.edl.cmd)) } :NONE"
  edl_script "${edl_script}")
file(WRITE "${EDL_LINKER}" "${edl_script}")

# The D01 is an alternative resident to Edelweiss's EBrew main.c; all ELF,
# firmware gating, cache coordination and native service sources are reused.
set(EDL_HOST_DIR "${edelweiss_SOURCE_DIR}/exword")
add_executable(toybox
  src/host.c src/host_fs.c src/io_bridge.c src/keymap.c src/terminal.c
  src/names.c src/manifest.c src/parser.c src/history.c
  "${EDL_HOST_DIR}/fs.c" "${EDL_HOST_DIR}/paths.c"
  "${EDL_HOST_DIR}/profile.c" "${EDL_HOST_DIR}/firmware.c"
  "${EDL_HOST_DIR}/scratch.c" "${EDL_HOST_DIR}/read_cache.c"
  "${EDL_HOST_DIR}/gint_exports.c" "${EDL_HOST_DIR}/native.S"
  "${EDL_HOST_DIR}/elf.c" "${EDL_HOST_DIR}/memory.c")
target_include_directories(toybox PRIVATE include "${EDL_HOST_DIR}" "${edelweiss_SOURCE_DIR}/sdk")
target_compile_definitions(toybox PRIVATE EDL_APPID="${EXWORD_APP_ID}")
target_compile_options(toybox PRIVATE -Wall -Wextra -Os -flto=auto
  -fno-pic -fno-pie -fno-stack-protector)
target_link_options(toybox PRIVATE -flto=auto -nostdlib -no-pie
  -Wl,--build-id=none -Wl,--no-warn-rwx-segments
  "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/toybox.map")
target_link_libraries(toybox PRIVATE Gint::Gint)
set_target_properties(toybox PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
generate_d01(TARGET toybox OUTPUT "${CMAKE_BINARY_DIR}/${EXWORD_MODULE}.d01"
  MODULE "${EXWORD_MODULE}")

foreach(bundle IN ITEMS base editors text)
  add_edl(${bundle} "src/${bundle}.c")
  target_include_directories(${bundle} PRIVATE include src)
endforeach()
target_sources(text PRIVATE src/busybox_text.c src/posix_tools.c)
target_sources(base PRIVATE src/posix.c)
target_sources(text PRIVATE src/posix.c)

if(SHELL_BUILD_PACKAGES)
  if(NOT EXWORD_METADATA_SOURCE)
    message(FATAL_ERROR "EXWORD_METADATA_SOURCE must point to the EX-word metadata repository")
  endif()
  generate_exword_package(TARGET toybox
    D01 "${CMAKE_BINARY_DIR}/${EXWORD_MODULE}.d01"
    APPID "${EXWORD_APP_ID}" TITLE "${EXWORD_APP_TITLE}"
    HTML_DIR "${EXWORD_METADATA_SOURCE}/html"
    OUTPUT_DIR "${CMAKE_BINARY_DIR}/packages" LOCALES ja cn)
  foreach(locale IN ITEMS ja cn)
    add_custom_target(toybox_modules_${locale} ALL
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:base>" "${CMAKE_BINARY_DIR}/packages/${locale}/${EXWORD_APP_ID}/base.edl"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:editors>" "${CMAKE_BINARY_DIR}/packages/${locale}/${EXWORD_APP_ID}/editors.edl"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:text>" "${CMAKE_BINARY_DIR}/packages/${locale}/${EXWORD_APP_ID}/text.edl"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.BUSYBOX"
        "${CMAKE_BINARY_DIR}/packages/${locale}/${EXWORD_APP_ID}/COPYING.TXT"
      DEPENDS toybox_package_${locale} base editors text VERBATIM)
  endforeach()
endif()
