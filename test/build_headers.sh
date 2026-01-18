#!/bin/sh

set -e

clang2py -l ./build/libgfgb.so ./src/common.h -o ./test/headers/common.py --clang-args="-I/usr/lib/clang/21/include/"
clang2py -l ./build/libgfgb.so ./src/cpu.h -o ./test/headers/cpu.py --clang-args="-I/usr/lib/clang/21/include/"
clang2py -l ./build/libgfgb.so ./src/disassemble.h -o ./test/headers/disassemble.py --clang-args="-I/usr/lib/clang/21/include/"
clang2py -l ./build/libgfgb.so ./src/ppu.h -o ./test/headers/ppu.py --clang-args="-I/usr/lib/clang/21/include/"
