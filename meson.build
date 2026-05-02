project(
  'gb_emu',
  'c', 'cpp', 'cython',
  meson_version : '>= 1.10.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23', 'cpp_std=gnu++23', 'python.install_env=auto'],
)

if get_option('enable_logging')
  add_global_arguments('-DGFGB_ENABLE_LOGGING=1', language : 'cpp')
endif
python_install_path = get_option('python_install')
if python_install_path != ''
  py = import('python').find_installation(python_install_path)
else
  py = import('python').find_installation()
endif
dep_py = py.dependency()

sdl3_dep = dependency('sdl3', required : false)

if not sdl3_dep.found()
  cmake = import('cmake')
  sdl_sub_pro_opts = cmake.subproject_options()
  sdl_sub_pro_opts.add_cmake_defines({'SDL_X11_XTEST' : false})
  sdl_sub_pro_opts.set_override_option('warning_level', '0')
  sdl_sub_pro = cmake.subproject('sdl', options : sdl_sub_pro_opts)
  sdl3_dep = sdl_sub_pro.dependency('SDL3_shared')
  meson.override_dependency('sdl3', sdl3_dep)
endif

catch_subpro = subproject('catch', default_options : {
    'install' : 'false',
    'tests' : 'false', # We don't care about running the catch internal tests
  },
)
catch_dep = catch_subpro.get_variable('catch2_with_main_dep')


imgui_dep = dependency('imgui-docking', default_options : {
    # For now i'm only using sdl3 and sdl3_renderer as backends. GLFW being enabled broke builds on my machine so I disabled everything i'm not using.
    'sdl3' : 'enabled',
    'sdl3_renderer' : 'enabled',
    'allegro5' : 'disabled',
    'dx10' : 'disabled',
    'dx11' : 'disabled',
    'dx12' : 'disabled',
    'dx9' : 'disabled',
    'glfw' : 'disabled',
    'metal' : 'disabled',
    'opengl' : 'disabled',
    'osx' : 'disabled',
    'sdl2' : 'disabled',
    'sdl2_renderer' : 'disabled',
    'sdl3_gpu' : 'disabled',
    'vulkan' : 'disabled',
    'webgpu' : 'disabled',
    'win' : 'disabled',
  },
)
gui_deps = [sdl3_dep, imgui_dep]
cli11_dep = dependency('CLI11')
tracy_dep = dependency(
  'tracy',
  static : true,
  default_options : {
    'warning_level' : '0',
  },
)
global_deps = [tracy_dep]

sources = [
  'src/common.cpp',
  'src/cpu.cpp',
  'src/disassemble.cpp',
  'src/gui.cpp',
  'src/mbc.cpp',
  'src/ppu.cpp',
  'src/apu.cpp',
  'src/timing.cpp',
]
include = include_directories(['src/', 'embed/'])

gfgb_so = shared_library(
  'gfgb',
  [sources],
  include_directories : include,
  dependencies : [gui_deps, global_deps],
)


gfgb_obj = static_library(
  'gfgb',
  'py/gfgb.pyx',
  include_directories : include,
  dependencies : [dep_py, global_deps, sdl3_dep],
  override_options : ['warning_level=0'],
)

gfgb_py_ext = py.extension_module(
  'gfgb',
  [sources],
  include_directories : include,
  link_whole : [gfgb_obj],
  install : true,
  dependencies : [gui_deps, global_deps],
)

exe = executable(
  'gb_emu',
  [sources, 'src/main.cpp'],
  include_directories : include,
  dependencies : [gui_deps, global_deps, cli11_dep],
  install : true,
)

cpu_test_exe = executable(
  'cpu_test_exe',
  ['test/unit/cpu_test.cpp'],
  link_with: gfgb_so,
  include_directories : [include, 'test/unit/'],
  dependencies : [gui_deps, global_deps, catch_dep],
)

mbc_test_exe = executable(
  'mbc_test_exe',
  ['test/unit/mbc_test.cpp'],
  link_with: gfgb_so,
  include_directories : [include, 'test/unit/'],
  dependencies : [gui_deps, global_deps, catch_dep],
)

ppu_test_exe = executable(
  'ppu_test_exe',
  ['test/unit/ppu_test.cpp'],
  link_with: gfgb_so,
  include_directories : [include, 'test/unit/'],
  dependencies : [gui_deps, global_deps, catch_dep],
)

disasm_test_exe = executable(
  'disasm_test_exe',
  ['test/unit/disassemble_test.cpp'],
  link_with: gfgb_so,
  include_directories : [include, 'test/unit/'],
  dependencies : [gui_deps, global_deps, catch_dep],
)

current_src_dir = meson.current_source_dir()

subdir('test/gf/layer_priority/')
subdir('test/gf/apu_ch1/')
subdir('test/gf/apu_ch1_timer/')

test('cpu_test', cpu_test_exe, workdir : current_src_dir)
test('mbc_test', mbc_test_exe, workdir : current_src_dir)
test('ppu_test', ppu_test_exe, workdir : current_src_dir)
test('disasm_test', disasm_test_exe, workdir : current_src_dir)

blargg_cpu_tests = [
  '01-special.gb',
  '02-interrupts.gb',
  '03-op sp,hl.gb',
  '04-op r,imm.gb',
  '05-op rp.gb',
  '06-ld r,r.gb',
  '07-jr,jp,call,ret,rst.gb',
  '08-misc instrs.gb',
  '09-op r,r.gb',
  '10-bit ops.gb',
  '11-op a,(hl).gb',
]

test_flags = ['--no_bootrom']
# I do not want any windows popping up or my 70 tests screaming at me during the bootrom.
test_env = ['SDL_VIDEODRIVER=dummy', 'SDL_AUDIODRIVER=dummy']

foreach test : blargg_cpu_tests
  test(
    'blargg_cpu_' + test,
    exe,
    args : ['exec', current_src_dir / 'test/blargg/cpu_instrs/individual' / test, '--test_mode', '--exec_speed=100', test_flags],
    env : test_env,
  )

endforeach

blargg_mem_timing_tests = [
  '01-read_timing.gb',
  '02-write_timing.gb',
  '03-modify_timing.gb',
]

foreach test : blargg_mem_timing_tests
  test(
    'blargg_mem_timing_' + test,
    exe,
    args : ['exec', current_src_dir / 'test/blargg/mem_timing/individual' / test, '--test_mode', '--exec_speed=100', test_flags],
    env : test_env,
  )
endforeach

test(
  'blargg_instr_timing',
  exe,
  args : ['exec', current_src_dir / 'test/blargg/instr_timing/instr_timing.gb', '--test_mode', '--exec_speed=100', test_flags],
  env : test_env,
)
test(
  'mooneye_vblank_stat_intr',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/acceptance/ppu/vblank_stat_intr-GS.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_rom_512kbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/rom_512kb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_rom_1mbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/rom_1Mb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_rom_2mbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/rom_2Mb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_rom_4mbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/rom_4Mb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_rom_8mbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/rom_8Mb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_rom_16mbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/rom_16Mb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'mooneye_mbc1_ram_64kbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/ram_64kb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)
test(
  'mooneye_mbc1_ram_256kbit',
  exe,
  args : [
    'exec', current_src_dir / 'test/mooneye/emulator-only/mbc1/ram_256kb.gb',
    '--test_mode',
    '--exec_speed=100',
    '--test_fail_regex=BBBBBB',
    '--test_pass_regex=\\x03\\x05\\x08\\x0D\\x15"',
    test_flags,
  ],
  env : test_env,
)

test(
  'single_step_tests',
  py,
  args : ['-mpytest', '--tap', current_src_dir / 'test/sst/'],
  depends : [gfgb_py_ext],
  protocol : 'tap',
)
