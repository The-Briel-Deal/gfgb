project('gb_layer_priority_test')

rgbasm  = find_program('rgbasm')
rgblink = find_program('rgblink')
rgbfix  = find_program('rgbfix')
rgbgfx  = find_program('rgbgfx')
python3 = find_program('python3')
sh      = find_program('sh')

# Build the layer_test.gb ROM.
#
# The two-pass link (steps below) is needed because bank_numbers.asm must be
# assembled with the final NB_BANKS value, which is only known after a first
# link pass.  The shell script mirrors the Makefile recipe exactly:
#   1. Convert crash_font.png -> .1bpp -> .pb8 + .pb8.size  (rgbgfx + pb8.py)
#   2. Assemble all regular sources -> obj/*.o
#   3. Assemble build_date.asm fresh (embeds __ISO_8601_UTC__)
#   4. First link pass -> temporary map file
#   5. Count used banks (nb_used_banks.py) -> NB_BANKS
#   6. Re-assemble bank_numbers.asm with -DNB_BANKS=<n>
#   7. Final link -> .gb + .map + .sym
#   8. rgbfix to write the ROM header
#
# Tool paths ($1-$5) and source/build dirs ($6-$7) are injected as positional
# arguments so the shell script never needs to locate them itself.
custom_target(
  'layer_test_rom',
  input: [
    'src/assets/crash_font.png',
    'src/intro.asm',
    'src/lib/crash_handler.asm',
    'src/lib/header.asm',
    'src/lib/memcpy_memset.asm',
    'src/lib/misc.asm',
    'src/lib/sgb.asm',
    'src/lib/unpb16.asm',
    'src/lib/vblank.asm',
    'src/lib/build_date.asm',
    'src/lib/bank_numbers.asm',
    'src/tools/nb_used_banks.py',
    'src/tools/pb8.py',
  ],
  output: 'layer_test.gb',
  command: [
    sh, '-c',
    '''
set -e
RGBASM=$1
RGBLINK=$2
RGBFIX=$3
RGBGFX=$4
PY=$5
S=$6
B=$7

ASFLAGS="-p 0xFF -I$S/src -I$S/include -I$B -Wall -Wextra"
LDFLAGS="-p 0xFF"
FIXFLAGS="-p 0xFF -i LTST -k HB -l 0x33 -m ROM_ONLY -n 0 -r 0x00 -t LAYER_TEST"

mkdir -p "$B/assets" "$B/obj/lib"

# 1. Asset pipeline: crash_font.png -> .1bpp -> .pb8 + .pb8.size
"$RGBGFX" -d 1 -o "$B/assets/crash_font.1bpp" "$S/src/assets/crash_font.png"
"$PY" "$S/src/tools/pb8.py" "$B/assets/crash_font.1bpp" "$B/assets/crash_font.1bpp.pb8"
FONT_SIZE=$(wc -c < "$B/assets/crash_font.1bpp")
echo "def NB_PB8_BLOCKS equ (($FONT_SIZE) + 7) / 8" > "$B/assets/crash_font.1bpp.pb8.size"

# 2. Assemble regular sources (crash_handler needs the generated assets above)
for f in intro lib/crash_handler lib/header lib/memcpy_memset lib/misc lib/sgb lib/unpb16 lib/vblank; do
  mkdir -p "$B/obj/$(dirname "$f")"
  "$RGBASM" $ASFLAGS -o "$B/obj/$f.o" "$S/src/$f.asm"
done

# 3. Assemble build_date.asm fresh each build (embeds current UTC timestamp)
"$RGBASM" $ASFLAGS -o "$B/obj/lib/build_date.o" "$S/src/lib/build_date.asm"

# 4. First link pass with bank_numbers.asm assembled without NB_BANKS (defaults to 256)
"$RGBASM" $ASFLAGS -o "$B/obj/lib/bank_numbers.o" "$S/src/lib/bank_numbers.asm"

OBJS="$B/obj/intro.o $B/obj/lib/crash_handler.o $B/obj/lib/header.o"
OBJS="$OBJS $B/obj/lib/memcpy_memset.o $B/obj/lib/misc.o $B/obj/lib/sgb.o"
OBJS="$OBJS $B/obj/lib/unpb16.o $B/obj/lib/vblank.o"
OBJS="$OBJS $B/obj/lib/build_date.o $B/obj/lib/bank_numbers.o"

"$RGBLINK" $LDFLAGS -m "$B/layer_test.map.tmp" $OBJS

# 5. Determine the actual number of used banks
NB_BANKS=$("$PY" "$S/src/tools/nb_used_banks.py" "$B/layer_test.map.tmp")
rm "$B/layer_test.map.tmp"

# 6. Re-assemble bank_numbers.asm with the real NB_BANKS value
"$RGBASM" $ASFLAGS -DNB_BANKS="$NB_BANKS" -o "$B/obj/lib/bank_numbers.o" "$S/src/lib/bank_numbers.asm"

# 7. Final link
"$RGBLINK" $LDFLAGS -m "$B/layer_test.map" -n "$B/layer_test.sym" -o "@OUTPUT@" $OBJS

# 8. Fix ROM header
"$RGBFIX" -v $FIXFLAGS "@OUTPUT@"
''',
    '_',
    rgbasm,
    rgblink,
    rgbfix,
    rgbgfx,
    python3,
    meson.current_source_dir(),
    meson.current_build_dir(),
  ],
  build_by_default: true,
)
