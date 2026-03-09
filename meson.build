project(
  'gb_emu',
  'c', 'cpp', 'cython',
  meson_version : '>= 1.3.0',
  version : '0.1',
  default_options : ['warning_level=3', 'c_std=c23', 'cpp_std=gnu++23', 'python.install_env=auto'],
)

if get_option('enable_logging')
  add_global_arguments('-DGFGB_ENABLE_LOGGING=1', language : 'cpp')
endif
if get_option('print_inst_during_exec')
  add_global_arguments('-DPRINT_INST_DURING_EXEC=1', language : 'cpp')
endif
python_install_path = get_option('python_install')
if python_install_path != ''
  py = import('python').find_installation(python_install_path)
else
  py = import('python').find_installation()
endif
dep_py = py.dependency()

cmake = import('cmake')
sdl_sub_pro_opts = cmake.subproject_options()
sdl_sub_pro_opts.add_cmake_defines({'SDL_X11_XTEST' : false})
sdl_sub_pro = cmake.subproject('sdl', options : sdl_sub_pro_opts)
sdl3_dep = sdl_sub_pro.dependency('SDL3_shared')
meson.override_dependency('sdl3', sdl3_dep)

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
  'src/disassemble.cpp',
  'src/common.cpp',
  'src/cpu.cpp',
  'src/ppu.cpp',
]
include = include_directories(['src/', 'embed/'])

shared_lib = shared_library(
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
  sources,
  cpp_args : '-DRUN_CPU_TESTS=1',
  include_directories : include,
  dependencies : [gui_deps, global_deps],
)

ppu_test_exe = executable(
  'ppu_test_exe',
  sources,
  cpp_args : '-DRUN_PPU_TESTS=1',
  include_directories : include,
  dependencies : [gui_deps, global_deps],
)

disasm_test_exe = executable(
  'disasm_test_exe',
  sources,
  cpp_args : '-DRUN_DISASSEMBLE_TESTS=1',
  include_directories : include,
  dependencies : [gui_deps, global_deps],
)

current_src_dir = meson.current_source_dir()

test('cpu_test', cpu_test_exe)
test('ppu_test', ppu_test_exe)
test('disasm_test', disasm_test_exe)
test(
  'blargg_cpu_01-special',
  exe,
  args : ['exec', current_src_dir / 'test/blargg/cpu_instrs/individual/01-special.gb', '--test_mode'],
  env : ['SDL_VIDEODRIVER=dummy'],
  timeout : 120, # Currently I need to increase the timeout for this to finish
                 # on the github actions CI runner since it's using the dummy
                 # video driver which is much slower since it does a bunch of
                 # rendering operations on the CPU. This can be sped up by just
                 # skipping all texture related SDL calls when using the dummy
                 # driver.
)
test(
  'single_step_tests',
  py,
  args : ['-mpytest', '--tap', current_src_dir / 'test/sst/'],
  depends : [gfgb_py_ext],
  protocol : 'tap',
)
