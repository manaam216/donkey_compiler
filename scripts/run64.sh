#!/usr/bin/env sh
#
# Build and run one Donkey-compiled program as an x86-64 Linux binary, and
# print its result.
#
# This exists because the development machine here is 32-bit MinGW, which
# cannot assemble or run x86-64 output. It cross-assembles with clang, links
# with ld.lld against the freestanding harness (no libc available locally), and
# executes under WSL. CI on Linux uses the normal libc path in scripts/test.sh.
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

mkdir -p "$work"

./build/donkey.exe "$src" "$work/$name.asm" >/dev/null

# The harness owns `main`, so rename the program's entry point.
sed 's/\bmain\b/donkey_main/g' "$work/$name.asm" > "$work/$name.s"

"$CLANG" --target=$TARGET -c "$work/$name.s" -o "$work/$name.o"

if [ ! -f "$work/harness.o" ]; then
    "$CLANG" --target=$TARGET -ffreestanding -nostdlib -O1 \
        -c tests/harness_freestanding.c -o "$work/harness.o"
fi

"$LLD" -o "$work/$name.elf" "$work/harness.o" "$work/$name.o"

# WSL here has no access to the Windows filesystem, so stream the binary in.
wsl.exe -d "$WSL_DISTRO" -e sh -c \
    'cat > /tmp/run64.elf; chmod +x /tmp/run64.elf; /tmp/run64.elf' \
    < "$work/$name.elf" | tr -d '\000'
