#!/usr/bin/env sh
#
# Build and run one Donkey-compiled program as an x86-64 Linux binary, and
# print its result.
#
# This exists because the development machine here is 32-bit MinGW, which
# cannot assemble or run x86-64 output. It cross-assembles with clang, links
# with ld.lld, and executes under WSL. CI on Linux does the same thing with the
# native toolchain, through scripts/test.sh.
#
# Two link modes:
#   - freestanding, for programs that call nothing from the system. These use
#     tests/harness_freestanding.c, which talks to the kernel directly, so no
#     libc is needed locally.
#   - against libc, for programs that call it. These borrow musl from the WSL
#     image and use tests/start.s as the entry point.
#
# Either way the program main is renamed and a harness prints its result, so
# the output has the same shape in both modes.
#
# Usage: sh scripts/run64.sh <source.c> [workdir]
set -eu

src="$1"
work="${2:-build/run64}"
name=$(basename "$src" .c)

CLANG="${CLANG:-/c/Program Files/LLVM/bin/clang.exe}"
LLD="${LLD:-/c/Program Files/LLVM/bin/ld.lld.exe}"
WSL_DISTRO="${WSL_DISTRO:-docker-desktop}"
TARGET=x86_64-unknown-linux-gnu
MUSL_TARGET=x86_64-unknown-linux-musl

mkdir -p "$work"

./build/donkey.exe "$src" -o "$work/$name.asm" >/dev/null

# Does this program call the C library?
if grep -qE '^[a-z].*\b(printf|puts|putchar|malloc|free|strlen|strcmp|memcpy)\b.*\(' "$src"; then
    needs_libc=1
else
    needs_libc=0
fi

# The harness owns main either way.
sed 's/\bmain\b/donkey_main/g' "$work/$name.asm" > "$work/$name.s"

if [ "$needs_libc" = "1" ]; then
    "$CLANG" --target=$MUSL_TARGET -c "$work/$name.s" -o "$work/$name.o"

    if [ ! -f "$work/libc.so.1" ]; then
        wsl.exe -d "$WSL_DISTRO" -e sh -c 'cat /lib/libc.musl-x86_64.so.1' \
            > "$work/libc.so.1"
    fi
    if [ ! -f "$work/start.o" ]; then
        "$CLANG" --target=$MUSL_TARGET -c tests/start.s -o "$work/start.o"
    fi
    if [ ! -f "$work/harness_libc.o" ]; then
        "$CLANG" --target=$MUSL_TARGET -c tests/harness_libc.c \
            -o "$work/harness_libc.o"
    fi

    # MSYS_NO_PATHCONV: this shell would otherwise rewrite the interpreter path
    # into a Windows one, and the loader would not be found at run time.
    MSYS_NO_PATHCONV=1 "$LLD" -o "$work/$name.elf" \
        "$work/start.o" "$work/harness_libc.o" "$work/$name.o" \
        "$work/libc.so.1" --dynamic-linker=/lib/ld-musl-x86_64.so.1
else
    "$CLANG" --target=$TARGET -c "$work/$name.s" -o "$work/$name.o"

    if [ ! -f "$work/harness.o" ]; then
        "$CLANG" --target=$TARGET -ffreestanding -nostdlib -O1 \
            -c tests/harness_freestanding.c -o "$work/harness.o"
    fi
    "$LLD" -o "$work/$name.elf" "$work/harness.o" "$work/$name.o"
fi

# WSL here has no access to the Windows filesystem, so stream the binary in.
wsl.exe -d "$WSL_DISTRO" -e sh -c \
    'cat > /tmp/run64.elf; chmod +x /tmp/run64.elf; /tmp/run64.elf' \
    < "$work/$name.elf" | tr -d '\000'
