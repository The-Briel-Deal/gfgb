#!/bin/sh

set -e

clang2py ./src/common.h -o ./test/headers/common.py --clang-args="-I/usr/lib/clang/21/include/"
clang2py ./src/cpu.h -o ./test/headers/cpu.py --clang-args="-I/usr/lib/clang/21/include/"
clang2py ./src/disassemble.h -o ./test/headers/disassemble.py --clang-args="-I/usr/lib/clang/21/include/"
clang2py ./src/ppu.h -o ./test/headers/ppu.py --clang-args="-I/usr/lib/clang/21/include/"
