sources = [
  'src/lib/bank_numbers.asm',
  'src/lib/build_date.asm',
  'src/lib/crash_handler.asm',
  'src/lib/header.asm',
  'src/lib/memcpy_memset.asm',
  'src/lib/misc.asm',
  'src/lib/sgb.asm',
  'src/lib/unpb16.asm',
  'src/lib/vblank.asm',
]

rgbasm = find_program('rgbasm')
rgblink = find_program('rgblink')
rgbfix = find_program('rgbfix')
rgbgfx = find_program('rgbgfx')

gb_fonts_1bpp = custom_target(
  'gb_fonts_1bpp',
  command: [rgbgfx, '-d', '1', '@INPUT@', '-o', '@OUTPUT@'],
  input: 'src/assets/crash_font.png',
  output: 'crash_font.1bpp',
  build_subdir: 'assets/',
)

gb_fonts_1bpp_size = custom_target(
  'gb_fonts_1bpp_size',
  command: [
    'printf',
    'def NB_PB8_BLOCKS equ ((%u) + 7) / 8\n', 
    '"$(wc -c <@INPUT@)"'
  ],
  capture: true,
  input: gb_fonts_1bpp,
  output: 'crash_font.1bpp.pb8.size',
  build_subdir: 'assets/',
)
current_build_dir = meson.current_build_dir()
gb_obj_generator = generator(rgbasm, 
  depends: [gb_fonts_1bpp, gb_fonts_1bpp_size],
  arguments: [
    '-p', '0xFF',
    '-I', '@CURRENT_SOURCE_DIR@' / 'src/',
    '-I', '@CURRENT_SOURCE_DIR@' / 'include/',
    '-I', current_build_dir,
    '-Wall',
    '-Wextra',
    '@INPUT@',
    '-o', '@OUTPUT@',
  ],
  output: '@BASENAME@.o',
)

layer_prio_test_unfixed = custom_target('layer_prio_test_unfixed',
  input: gb_obj_generator.process(sources),
  command: [rgblink, '-p', '0xFF', '@INPUT@', '-o', '@OUTPUT@'],
  output: 'layer_prio_test_unfixed.gb',
)

custom_target('layer_prio_test',
  command: [
    rgbfix,
    '-v',
    '-p', '0xFF',
    '-i', 'LTST',
    '-k', 'HB',
    '-l', '0x33',
    '-m', 'ROM_ONLY',
    '-n', '0',
    '-r', '0x00',
    '-t', 'LAYER_TEST',
    layer_prio_test_unfixed,
    '-o', '@OUTPUT@',
  ],
  output: 'layer_prio_test.gb',
  build_by_default: true,
)
