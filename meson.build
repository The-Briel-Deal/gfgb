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
