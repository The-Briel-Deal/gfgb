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

gb_obj_generator = generator(rgbasm, 
  arguments: [
    '-p', '0xFF',
    '-I', '@CURRENT_SOURCE_DIR@' / 'src/',
    '-I', '@CURRENT_SOURCE_DIR@' / 'include/',
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
