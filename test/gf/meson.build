common_sources = [
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

rgbasm = find_program('rgbasm', required: false)
rgblink = find_program('rgblink', required: false)
rgbfix = find_program('rgbfix', required: false)
rgbgfx = find_program('rgbgfx', required: false)

if rgbasm.found() and rgblink.found() and rgbfix.found() and rgbgfx.found()
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
  gb_fonts_1bpp_pb8 = custom_target(
    'gb_fonts_1bpp_pb8',
    command: [
      'python', '@CURRENT_SOURCE_DIR@' / 'src/tools/pb8.py', '@INPUT@', '@OUTPUT@',
    ],
    input: gb_fonts_1bpp,
    output: 'crash_font.1bpp.pb8',
    build_subdir: 'assets/',
  )
  current_build_dir = meson.current_build_dir()
  gb_obj_generator = generator(rgbasm, 
    depends: [gb_fonts_1bpp_pb8, gb_fonts_1bpp_size],
    arguments: [
      '-p', '0xFF',
      '-I', '@CURRENT_SOURCE_DIR@' / 'src/',
      '-I', '@CURRENT_SOURCE_DIR@' / 'include/',
      '-I', current_build_dir,
      '-Wall',
      '-Wextra',
      '-DNB_BANKS=2',
      '@INPUT@',
      '-o', '@OUTPUT@',
    ],
    output: '@BASENAME@.o',
  )

  tests = ['apu_ch1_timer', 'apu_ch1', 'layer_prio', 'apu_ch1_env_sweep']
  
  foreach test : tests
    test_unfixed = custom_target(test + '_test_unfixed',
      input: gb_obj_generator.process(['src/' + test + '.asm', common_sources]),
      command: [rgblink, '-p', '0xFF', '@INPUT@', '-o', '@OUTPUT0@', '--sym', '@OUTPUT1@'],
      output: [test + '_test_unfixed.gb', test + '_test.sym'],
    )
    
    custom_target(test + '_test',
      command: [
        rgbfix,
        '-v',
        '-p', '0xFF',
        '-k', 'HB',
        '-l', '0x33',
        '-m', 'ROM_ONLY',
        '-n', '0',
        '-r', '0x00',
        test_unfixed,
        '-o', '@OUTPUT@',
      ],
      output: test + '_test.gb',
      build_by_default: true,
    )
  endforeach
endif
