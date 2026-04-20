rgbasm = find_program('rgbasm')
rgblink = find_program('rgblink')
rgbfix = find_program('rgbfix')

intro = custom_target('intro',
  input:['src/intro.asm'],
  command: [
    rgbasm,
    '-p', '0xFF',
    '-I', '@CURRENT_SOURCE_DIR@' / 'src/',
    '-I', '@CURRENT_SOURCE_DIR@' / 'include/',
    '-Wall',
    '-Wextra',
    '@INPUT@',
    '-o', '@OUTPUT@',
  ],
  output: ['intro.o'],
)
memcpy_memset = custom_target('memcpy_memset',
  input:['src/lib/memcpy_memset.asm'],
  command: [
    rgbasm,
    '-p', '0xFF',
    '-I', '@CURRENT_SOURCE_DIR@' / 'src/',
    '-I', '@CURRENT_SOURCE_DIR@' / 'include/',
    '-Wall',
    '-Wextra',
    '@INPUT@',
    '-o', '@OUTPUT@',
  ],
  output: ['memcpy_memset.o'],
)

layer_prio_test_unfixed = custom_target('layer_prio_test_unfixed',
  command: [rgblink, '-p', '0xFF', intro, memcpy_memset, '-o', '@OUTPUT@'],
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
