project(
  'gb_emu',
  'c', 'cython',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23'],
)

dependencies = dependency('sdl3')

sources = [
  'src/disassemble.c',
  'src/common.c',
  'src/cpu.c',
  'src/ppu.c',
]

shared_lib = shared_library(
  'gfgb', 
  [sources],
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
)

py = import('python').find_installation()
dep_py = py.dependency()

gfgb_obj = static_library(
  'gfgb_obj',
  'py/gfgb.pyx',
  cython_args : ['--module-name=gfgb_py'],
  include_directories : include_directories(['src/']),
  dependencies : [dep_py],
  override_options : ['warning_level=0'],
)

gfgb_py_ext = py.extension_module(
  'gfgb_py',
  [sources],
  include_directories : include_directories(['src/']),
  link_whole : [gfgb_obj],
  dependencies : [dependencies],
)

gfgb_py_testing_ext = py.extension_module(
  'gfgb_py_testing',
  [sources],
  c_args : ['-DRUN_CPU_TESTS=1', '-DUSE_FLAT_RAM_FOR_TESTING=1'],
  include_directories : include_directories(['src/']),
  link_whole : [gfgb_obj],
  dependencies : [dependencies],
)

pytest = find_program('pytest')

exe = executable(
  'gb_emu',
  [sources, 'src/main.c'],
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

cpu_test_exe = executable(
  'cpu_test_exe',
  sources,
  c_args : '-DRUN_CPU_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

ppu_test_exe = executable(
  'ppu_test_exe',
  sources,
  c_args : '-DRUN_PPU_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

disasm_test_exe = executable(
  'disasm_test_exe',
  sources,
  c_args : '-DRUN_DISASSEMBLE_TESTS=1',
  include_directories : include_directories(['src/']),
  dependencies : dependencies,
  install : true,
)

current_src_dir = meson.current_source_dir()

test('cpu_test', cpu_test_exe)
test('ppu_test', ppu_test_exe)
test('disasm_test', disasm_test_exe)
test(
  'single_step_tests',
  pytest,
  args : ['--tap', '--gfgb-py-ext', gfgb_py_testing_ext, current_src_dir / 'test/sst/'],
  depends : [gfgb_py_testing_ext],
  protocol : 'tap',
)

rgbasm = find_program('rgbasm')
rgblink = find_program('rgblink')
rgbfix = find_program('rgbfix')
rgbgfx = find_program('rgbgfx')
aseprite = find_program('aseprite')


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
