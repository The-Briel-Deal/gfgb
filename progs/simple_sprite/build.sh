#!/bin/sh

set -e

rgbasm -I /data/Code/gb_emu/progs/gb_include/ -I /data/Code/gb_emu/assets/ ./simple_sprite.asm -o simple_sprite.o
rgblink ./simple_sprite.o -o simple_sprite.gb -m simple_sprite.map -n simple_sprite.sym
rgbfix -v -p 0xFF simple_sprite.gb

