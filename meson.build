project(
  'gb_emu',
  'c',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23'],
)

dependencies = dependency('sdl3')

sources = [
  'src/disassemble.c',
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
  c_args : '-DRUN_CPU_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

ppu_test_exe = executable(
  'ppu_test_exe',
  ['src/cpu.c', 'src/disassemble.c','src/ppu.c'],
  c_args : '-DRUN_PPU_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

disasm_test_exe = executable(
  'disasm_test_exe',
  ['src/cpu.c', 'src/disassemble.c'],
  c_args : '-DRUN_DISASSEMBLE_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

test('cpu_test', cpu_test_exe)
test('ppu_test', ppu_test_exe)
test('disasm_test', disasm_test_exe)

rgbasm = find_program('rgbasm')
rgblink = find_program('rgblink')
rgbfix = find_program('rgbfix')
rgbgfx = find_program('rgbgfx')
aseprite = find_program('aseprite')

current_src_dir = meson.current_source_dir()

doggo_png = custom_target('doggo_png',
  output : 'doggo.png',
  input : 'assets/doggo.aseprite',
  command : [aseprite, '@INPUT@', '--save-as=@OUTPUT@', '--batch'],
)

doggo_bin = custom_target('doggo_bin',
  output : 'doggo.bin',
  input : doggo_png,
  command : [rgbgfx, '@INPUT@', '-o', '@OUTPUT@'],
)

simple_sprite_obj = custom_target('simple_sprite_obj',
  output : 'simple_sprite.o',
  input : 'progs/simple_sprite/simple_sprite.asm',
  depends : doggo_bin,
  command : [rgbasm, '-I', current_src_dir / 'progs/gb_include/', '@INPUT@', '-o', '@OUTPUT@'],
)

simple_sprite_unfixed_gb = custom_target('simple_sprite_unfixed_gb',
  output : 'simple_sprite_unfixed.gb',
  input : simple_sprite_obj,
  command : [rgblink, '@INPUT@', '-o', '@OUTPUT@', '-m', 'simple_sprite.map', '-n', 'simple_sprite.sym'],
)

simple_sprite_gb = custom_target('simple_sprite_gb',
  output : 'simple_sprite.gb',
  input : simple_sprite_unfixed_gb,
  command : [rgbfix, '-v', '-p', '0xFF', '@INPUT@', '-o', '@OUTPUT@'],
  install : true,
  install_dir : 'simple_sprite',
)
