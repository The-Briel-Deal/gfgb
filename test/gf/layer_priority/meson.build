project('gb_layer_priority_test')

gb_compiler = generator(find_program('rgbasm'),
  arguments: [
    '-p', '0xFF',
    '-Isrc/',
    '-Iinclude/',
    '-Wall',
    '-Wextra',
    '@INPUT@',
    '-o', '@OUTPUT@',
  ],
  output: '@BASENAME@.o',
)
intro_obj = gb_compiler.process('src/intro.asm')
build_target('intro_obj', intro_obj)

# gb_linker = generator(find_program('rgblink'))
# gb_fixer = generator(find_program('rgbfix'))
# gb_tilegen = generator(find_program('rgbfx'))

# [I] gf@arch:~/C/g/t/g/layer_priority=>make -n
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/intro.o src/intro.asm \
#     -M obj/intro.mk -MG -MP -MQ obj/intro.o -MQ obj/intro.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/bank_numbers.o src/lib/bank_numbers.asm \
#     -M obj/lib/bank_numbers.mk -MG -MP -MQ obj/lib/bank_numbers.o -MQ obj/lib/bank_numbers.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/build_date.o src/lib/build_date.asm \
#     -M obj/lib/build_date.mk -MG -MP -MQ obj/lib/build_date.o -MQ obj/lib/build_date.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/crash_handler.o src/lib/crash_handler.asm \
#     -M obj/lib/crash_handler.mk -MG -MP -MQ obj/lib/crash_handler.o -MQ obj/lib/crash_handler.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/header.o src/lib/header.asm \
#     -M obj/lib/header.mk -MG -MP -MQ obj/lib/header.o -MQ obj/lib/header.mk
# warning: Edit to set palettes here [-Wuser]
#     at src/lib/header.asm(57)
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/memcpy_memset.o src/lib/memcpy_memset.asm \
#     -M obj/lib/memcpy_memset.mk -MG -MP -MQ obj/lib/memcpy_memset.o -MQ obj/lib/memcpy_memset.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/misc.o src/lib/misc.asm \
#     -M obj/lib/misc.mk -MG -MP -MQ obj/lib/misc.o -MQ obj/lib/misc.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/sgb.o src/lib/sgb.asm \
#     -M obj/lib/sgb.mk -MG -MP -MQ obj/lib/sgb.o -MQ obj/lib/sgb.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/unpb16.o src/lib/unpb16.asm \
#     -M obj/lib/unpb16.mk -MG -MP -MQ obj/lib/unpb16.o -MQ obj/lib/unpb16.mk
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/vblank.o src/lib/vblank.asm \
#     -M obj/lib/vblank.mk -MG -MP -MQ obj/lib/vblank.o -MQ obj/lib/vblank.mk
# rgbgfx -d 1 -o assets/crash_font.1bpp src/assets/crash_font.png
# printf 'def NB_PB8_BLOCKS equ ((%u) + 7) / 8\n' \
#     "$(wc -c <assets/crash_font.1bpp)" >assets/crash_font.1bpp.pb8.size
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/crash_handler.o src/lib/crash_handler.asm \
#     -M obj/lib/crash_handler.mk -MG -MP -MQ obj/lib/crash_handler.o -MQ obj/lib/crash_handler.mk
# rm assets/crash_font.1bpp
# src/tools/pb8.py assets/crash_font.1bpp assets/crash_font.1bpp.pb8
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/crash_handler.o src/lib/crash_handler.asm \
#     -M obj/lib/crash_handler.mk -MG -MP -MQ obj/lib/crash_handler.o -MQ obj/lib/crash_handler.mk
# mkdir -p "bin"
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -o obj/lib/build_date.o src/lib/build_date.asm
# rgblink -p 0xFF -m bin/layer_test.map.tmp obj/intro.o obj/lib/bank_numbers.o obj/lib/build_date.o obj/lib/crash_handler.o obj/lib/header.o obj/lib/memcpy_memset.o obj/lib/misc.o obj/lib/sgb.o obj/lib/unpb16.o obj/lib/vblank.o
# NB_BANKS=$(src/tools/nb_used_banks.py bin/layer_test.map.tmp) \
# && rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra -DNB_BANKS=$NB_BANKS \
#     -o obj/lib/bank_numbers.o src/lib/bank_numbers.asm
# rm bin/layer_test.map.tmp
# rgblink -p 0xFF -m bin/layer_test.map -n bin/layer_test.sym -o bin/layer_test.gb obj/intro.o obj/lib/bank_numbers.o obj/lib/build_date.o obj/lib/crash_handler.o obj/lib/header.o obj/lib/memcpy_memset.o obj/lib/misc.o obj/lib/sgb.o obj/lib/unpb16.o obj/lib/vblank.o \
# && rgbfix -v -p 0xFF -i "LTST" -k "HB" -l 0x33 -m ROM_ONLY -n 0 -r 0x00 -t LAYER_TEST bin/layer_test.gb
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/intro.asm -DPRINT_DEBUGFILE >obj/intro.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/bank_numbers.asm -DPRINT_DEBUGFILE >obj/lib/bank_numbers.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/build_date.asm -DPRINT_DEBUGFILE >obj/lib/build_date.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/crash_handler.asm -DPRINT_DEBUGFILE >obj/lib/crash_handler.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/header.asm -DPRINT_DEBUGFILE >obj/lib/header.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/memcpy_memset.asm -DPRINT_DEBUGFILE >obj/lib/memcpy_memset.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/misc.asm -DPRINT_DEBUGFILE >obj/lib/misc.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/sgb.asm -DPRINT_DEBUGFILE >obj/lib/sgb.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/unpb16.asm -DPRINT_DEBUGFILE >obj/lib/unpb16.dbg
# rgbasm -p 0xFF -Isrc/ -Iinclude/ -Wall -Wextra src/lib/vblank.asm -DPRINT_DEBUGFILE >obj/lib/vblank.dbg
# mkdir -p "bin"
# echo @debugfile 1.0 > bin/layer_test.dbg
# printf '@include "../%s"\n' obj/intro.dbg obj/lib/bank_numbers.dbg obj/lib/build_date.dbg obj/lib/crash_handler.dbg obj/lib/header.dbg obj/lib/memcpy_memset.dbg obj/lib/misc.dbg obj/lib/sgb.dbg obj/lib/unpb16.dbg obj/lib/vblank.dbg >> bin/layer_test.dbg
