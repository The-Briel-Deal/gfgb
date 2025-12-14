project(
  'gb_emu',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23'],
)

dependencies = dependency('sdl3')
sources = [
  'src/cpu.c',
  'src/main.c',
]

exe = executable(
  'gb_emu',
  [sources],
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

cpu_test_exe = executable(
  'cpu_test_exe',
  ['src/cpu.c'],
  c_args : '-DRUN_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

test('cpu tests', cpu_test_exe)

rgbasm = find_program('rgbasm')
rgblink = find_program('rgblink')
rgbfix = find_program('rgbfix')

simple_sprite_obj = custom_target('simple_sprite_obj',
  output : 'simple_sprite.o',
  input : 'progs/simple_sprite/simple_sprite.asm',
  command: [rgbasm, '@INPUT@', '-o', '@OUTPUT@'],
  install : true,
  install_dir : 'simple_sprite',
)
simple_sprite_unfixed_gb = custom_target('simple_sprite_unfixed_gb',
  output : 'simple_sprite_unfixed.gb',
  input : simple_sprite_obj,
  command: [rgblink, '@INPUT@', '-o', '@OUTPUT@'],
  install : true,
  install_dir : 'simple_sprite',
)

simple_sprite_gb = custom_target('simple_sprite_gb',
  output : 'simple_sprite.gb',
  input : simple_sprite_unfixed_gb,
  command: [rgbfix, '-v', '-p', '0xFF', '@INPUT@', '-o', '@OUTPUT@'],
  install : true,
  install_dir : 'simple_sprite',
)
