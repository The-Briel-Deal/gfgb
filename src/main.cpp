#include "common.h"
#include "cpu.h"
#include "disassemble.h"
#include "ppu.h"

#include <tracy/Tracy.hpp>
/* enable file permission validators which are locked behind this ifdef for the time being */
#define CLI11_ENABLE_EXTRA_VALIDATORS 1
#include <CLI/CLI.hpp>
/* use the callbacks instead of main() */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <regex>
#include <vector>

enum run_mode {
  UNSET = 0,
  EXECUTE,
  DISASSEMBLE,
};

static bool gb_load_rom(struct gb_state *gb_state, const char *rom_name, const char *bootrom_name,
                        const char *sym_name) {
  FILE   *f;
  int     err;
  uint8_t bytes[KB(16)];
  int     bytes_len;

  // Load ROM into gb_state->rom0 (rom is optional since the disassembler can
  // also assemble only the boot rom).
  if (rom_name != NULL) {
    // TODO: Load into multiple banks once bank switching is added.
    f         = fopen(rom_name, "r");
    bytes_len = fread(bytes, sizeof(uint8_t), KB(16), f);
    if ((err = ferror(f))) {
      LogCritical("Error when reading rom file: %d", err);
      return false;
    }
    memcpy(gb_state->ram.rom0, bytes, bytes_len);
    if (!feof(f)) {
      bytes_len = fread(bytes, sizeof(uint8_t), KB(16), f);
      if ((err = ferror(f))) {
        LogCritical("Error when reading rom file: %d", err);
        return false;
      }
      memcpy(gb_state->ram.rom1, bytes, bytes_len);
    }
    fclose(f);
    gb_state->rom_loaded = true;
  }

  // Load debug symbols into gb_state->syms (symbols are optional)
  if (sym_name != NULL) {
    alloc_symbol_list(&gb_state->syms);
    f = fopen(sym_name, "r");
    parse_syms(&gb_state->syms, f);
    if ((err = ferror(f))) {
      LogCritical("Error when reading symbol file: %d", err);
      return false;
    }
    fclose(f);
  }
  gb_state_load_bootrom(gb_state, bootrom_name);

  return true;
}

// Returns true on success, if error occured when opening the file return false.
bool gb_setup_serial_out(gb_state_t *gb_state, const char *serial_output_filename) {
  if (serial_output_filename != NULL) {
    gb_state->serial_port_output_file = fopen(serial_output_filename, "w");
    if (gb_state->serial_port_output_file == NULL) {
      LogCritical("Error when opening serial port output file: %s", strerror(errno));
      return false;
    }
  }
  return true;
}
bool gb_setup_exec_tracing(gb_state_t *gb_state, const char *trace_exec_filename) {
  if (trace_exec_filename != NULL) {
    gb_state->dbg_trace_exec_fout = fopen(trace_exec_filename, "w");
    if (gb_state->dbg_trace_exec_fout == NULL) {
      LogCritical("An error occured when opening file with name '%s': %s", trace_exec_filename, strerror(errno));
      return false;
    }
  } else {
    gb_state->dbg_trace_exec_fout = stdout;
  }
  return true;
}

bool gb_set_breakpoint(gb_state_t *gb_state, const char *bp_str, int bp_str_len) {
  if (bp_str_len == 5 && bp_str[0] == '$') {
    char    *endptr;
    uint16_t bp_addr = strtoul(bp_str + 1, &endptr, 16);
    if ((endptr - bp_str) != 5) {
      LogCritical("'%.*s' is not a valid 16 bit hex addr.", bp_str_len, bp_str);
      return false;
    }
    gb_breakpoint_t bp = {.addr = bp_addr};
    gb_state->breakpoints->push_back(bp);
    return true;
  }
  // TODO: Add support for breakpoints at symbols.
  LogCritical("'%.*s' is not a valid breakpoint string, currently only 16 bit hex addr's are supported.", bp_str_len,
              bp_str);
  return false;
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  gb_state_t *gb_state = gb_state_alloc();
  *appstate            = gb_state;
  gb_state_init(gb_state);

  enum run_mode run_mode = UNSET;

  CLI::App      gb_cli("A GameBoy emulator by Gabriel Ford", "GFGB");

  gb_cli.require_subcommand(1, 1);

  CLI::App   *gb_cli_exec   = gb_cli.add_subcommand("exec", "Execute GameBoy ROM");
  CLI::App   *gb_cli_disasm = gb_cli.add_subcommand("disasm", "Disassemble GameBoy ROM");

  std::string rom_filename;
  gb_cli_exec
      ->add_option("rom_file", rom_filename) // (these comments are just here to make clang-fmt break these calls)
      ->required()
      ->check(CLI::ExistingFile)
      ->check(CLI::ReadPermissions);
  gb_cli_disasm
      ->add_option("rom_file", rom_filename) //
      ->required()
      ->check(CLI::ExistingFile)
      ->check(CLI::ReadPermissions);

  std::string symbol_filename;
  gb_cli_exec
      ->add_option("-s,--sym_file", symbol_filename) //
      ->check(CLI::ExistingFile)
      ->check(CLI::ReadPermissions);
  gb_cli_disasm
      ->add_option("-s,--sym_file", symbol_filename) //
      ->check(CLI::ExistingFile)
      ->check(CLI::ReadPermissions);

  std::string bootrom_filename;
  gb_cli_exec
      ->add_option("-b,--bootrom_file", bootrom_filename) //
      ->check(CLI::ExistingFile)
      ->check(CLI::ReadPermissions);
  gb_cli_disasm
      ->add_option("-b,--bootrom_file", bootrom_filename) //
      ->check(CLI::ExistingFile)
      ->check(CLI::ReadPermissions);

  gb_cli_exec->add_option("--exec_speed", gb_state->dbg_speed_factor);

  gb_cli_exec->add_flag(
      "--trace_exec", gb_state->dbg_trace_exec,
      "Print instructions to file/stdout as the rom is executing. This can be toggled at runtime in Debug UI as well.");
  std::string trace_exec_filename;
  gb_cli_exec->add_option("--trace_out", trace_exec_filename,
                          "File to write trace to if execution tracing is enabled.");

  // TODO: Bank should be specifiable as well.
  std::vector<std::string> breakpoints;
  gb_cli_exec
      ->add_option(
          "--bp,--breakpoint", breakpoints,
          "A breakpoint identifier, can be a GB 16 bit addr in hex if prefixed with `$`, or a debug symbol name.")
      ->expected(0, -1);

  gb_cli_exec->add_flag("-p,--paused", gb_state->execution_paused, "Start emulator execution paused.");
  gb_cli_exec->add_flag("-t,--test_mode", gb_state->test_mode,
                        "Run emulator in automated test mode, this is mostly just used to automatically detect if a "
                        "test rom passed or failed.");
  std::string test_mode_pass_regex;
  gb_cli_exec
      ->add_option("--test_pass_regex", test_mode_pass_regex,
                   "Used with --test_mode flag, the regex exp to scan the serial_port output for to detect success.")
      ->default_val<const char *>(".*Passed.*");
  std::string test_mode_fail_regex;
  gb_cli_exec
      ->add_option("--test_fail_regex", test_mode_fail_regex,
                   "Used with --test_mode flag, the regex exp to scan the serial_port output for to detect failure.")
      ->default_val<const char *>(".*Failed.*");

  // TODO: There isn't a good built in validator for an output file that may or may not exist. Maybe i'll want to add
  // one down the road?
  std::string serial_output_filename;
  gb_cli_exec->add_option( // Exclusively used for the exec subcommand since nothing is executed in disasm
      "-P,--serial_port", serial_output_filename,
      "Filepath to output all data written to SB IO Reg (0xFF01). Used for logging in some test roms.");

  try {
    gb_cli.parse(argc, argv);
  } catch (const CLI ::ParseError &e) {
    gb_cli.exit(e);
    return SDL_APP_FAILURE;
  }
  if (gb_state->test_mode) {
    gb_state->compiled_pass_regex = new std::basic_regex(test_mode_pass_regex);
    gb_state->compiled_fail_regex = new std::basic_regex(test_mode_fail_regex);
  }

  for (std::string bp : breakpoints) {
    if (!gb_set_breakpoint(gb_state, bp.c_str(), bp.length())) {
      return SDL_APP_FAILURE;
    }
  }

  const char *rom_filename_cstr    = rom_filename.c_str();
  const char *symbol_filename_cstr = NULL;
  if (symbol_filename.length() != 0) {
    symbol_filename_cstr = symbol_filename.c_str();
  }
  const char *bootrom_filename_cstr = NULL;
  if (bootrom_filename.length() != 0) {
    bootrom_filename_cstr = bootrom_filename.c_str();
  }
  const char *trace_exec_filename_cstr = NULL;
  if (trace_exec_filename.length() != 0) {
    trace_exec_filename_cstr = trace_exec_filename.c_str();
  }
  const char *serial_output_filename_cstr = NULL;
  if (serial_output_filename.length() != 0) {
    serial_output_filename_cstr = serial_output_filename.c_str();
  }

  if (gb_cli_exec->parsed()) {
    run_mode = EXECUTE;
  }
  if (gb_cli_disasm->parsed()) {
    run_mode = DISASSEMBLE;
  }

  if (!gb_load_rom(gb_state, rom_filename_cstr, bootrom_filename_cstr, symbol_filename_cstr)) return SDL_APP_FAILURE;
  SDL_SetAppMetadata("GF-GB", "0.0.1", "com.gf.gameboy-emu");

  switch (run_mode) {
  case EXECUTE: {
    if (!gb_setup_serial_out(gb_state, serial_output_filename_cstr)) return SDL_APP_FAILURE;
    if (!gb_setup_exec_tracing(gb_state, trace_exec_filename_cstr)) return SDL_APP_FAILURE;
    if (!gb_video_init(gb_state)) return SDL_APP_FAILURE;
    return SDL_APP_CONTINUE; /* carry on with the program! */
  };
  case DISASSEMBLE: {
    disassemble(gb_state, stdout);

    return SDL_APP_SUCCESS;
  }
  case UNSET:
    fprintf(stderr, "Run Mode unset, please specify either `exec` to execute or "
                    "`disasm` to disassemble.\n");
    return SDL_APP_FAILURE;
  default: return SDL_APP_FAILURE;
  }
}

void handle_key_event(struct gb_state *gb_state, const SDL_KeyboardEvent *event) {
  (void)gb_state;
  switch (event->type) {
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_KEY_DOWN: {
    // TODO: I should expose this as user-changable conf
    switch (event->key) {
    case SDLK_W: gb_state->joy_pad_state.dpad_up = (event->type == SDL_EVENT_KEY_DOWN); break;
    case SDLK_A: gb_state->joy_pad_state.dpad_left = (event->type == SDL_EVENT_KEY_DOWN); break;
    case SDLK_S: gb_state->joy_pad_state.dpad_down = (event->type == SDL_EVENT_KEY_DOWN); break;
    case SDLK_D: gb_state->joy_pad_state.dpad_right = (event->type == SDL_EVENT_KEY_DOWN); break;

    case SDLK_U: gb_state->joy_pad_state.button_a = (event->type == SDL_EVENT_KEY_DOWN); break;
    case SDLK_I: gb_state->joy_pad_state.button_b = (event->type == SDL_EVENT_KEY_DOWN); break;
    case SDLK_O: gb_state->joy_pad_state.button_start = (event->type == SDL_EVENT_KEY_DOWN); break;
    case SDLK_P: gb_state->joy_pad_state.button_select = (event->type == SDL_EVENT_KEY_DOWN); break;
    }
    break;
  }
  default: unreachable();
  }
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  gb_state_t *gb_state = (gb_state_t *)appstate;

  // don't let ImGui swallow quit events
  if (event->type == SDL_EVENT_QUIT) return SDL_APP_SUCCESS;

  if (gb_video_handle_sdl_event(gb_state, event)) return SDL_APP_CONTINUE;

  switch (event->type) {
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_KEY_DOWN: handle_key_event(gb_state, &event->key); break;
  case SDL_EVENT_WINDOW_RESIZED: /* no action should be needed since the the logical representation is the gb width x
                                    height, screen will be automatically letter boxed on resize */
    break;
  }

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

static void gb_update_io_joyp(gb_state_t *gb_state) {
  uint8_t *io_joyp          = &gb_state->regs.io.joyp;
  uint8_t  new_lower_nibble = 0x0F;
  if (((*io_joyp) >> 4 & 0b11) == 0b10) {
    // D-Pad selected
    if (gb_state->joy_pad_state.dpad_right) new_lower_nibble &= ~JOYP_D_PAD_RIGHT;
    if (gb_state->joy_pad_state.dpad_left) new_lower_nibble &= ~JOYP_D_PAD_LEFT;
    if (gb_state->joy_pad_state.dpad_up) new_lower_nibble &= ~JOYP_D_PAD_UP;
    if (gb_state->joy_pad_state.dpad_down) new_lower_nibble &= ~JOYP_D_PAD_DOWN;
  }
  if (((*io_joyp) >> 4 & 0b11) == 0b01) {
    // Buttons selected
    if (gb_state->joy_pad_state.button_a) new_lower_nibble &= ~JOYP_BUTTON_A;
    if (gb_state->joy_pad_state.button_b) new_lower_nibble &= ~JOYP_BUTTON_B;
    if (gb_state->joy_pad_state.button_select) new_lower_nibble &= ~JOYP_BUTTON_SELECT;
    if (gb_state->joy_pad_state.button_start) new_lower_nibble &= ~JOYP_BUTTON_START;
  }

  // only keep bits 5 and 4 (select buttons and select d-pad)
  *io_joyp &= 0x30;
  *io_joyp |= new_lower_nibble;
  *io_joyp |= 0xC0; // most significant two bits are set high since they are unused
}

static void check_breakpoints(gb_state_t *gb_state, uint16_t prev_pc, uint16_t curr_pc) {
  ZoneScopedN("Check Breakpoints");
  for (gb_breakpoint_t bp : *gb_state->breakpoints) {
    if (bp.addr >= prev_pc && bp.addr < curr_pc) {
      gb_state->execution_paused = true;
    }
  }
}

void print_serial_port_escaped(gb_state_t *gb_state) {
  for (char c : *gb_state->serial_port_output_string) {
    if (isprint(c)) {
      putchar(c);
    } else {
      printf("\\x%.2X", c);
    }
  }
}

#ifdef TRACY_ENABLE
const char *const TracyFrame_SDL_AppIterate = "App Iteration";
#endif

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
  FrameMarkStart(TracyFrame_SDL_AppIterate);
  gb_state_t *gb_state = (gb_state_t *)appstate;

  if (gb_state->test_mode) {
    if (std::regex_search(*gb_state->serial_port_output_string, *gb_state->compiled_pass_regex)) {
      printf("Test succeeded with serial port output:\n");
      print_serial_port_escaped(gb_state);
      return SDL_APP_SUCCESS;
    }
    if (std::regex_search(*gb_state->serial_port_output_string, *gb_state->compiled_fail_regex)) {
      printf("Test failed with serial port output:\n");
      print_serial_port_escaped(gb_state);
      return SDL_APP_FAILURE;
    }
  }

  uint64_t prev_ns_elapsed_total = gb_state->ns_elapsed_total;
  gb_state->ns_elapsed_total     = SDL_GetTicksNS();
  if ((!gb_state->execution_paused) || (gb_state->dbg_step_inst_count > 0)) {
    // We only increment this timer when execution hasn't been paused for debugging. If I just used the result of
    // SDL_GetTicksNS() then execution would run super fast after resuming to catch up with the timer.
    gb_state->ns_elapsed_while_running +=
        ((gb_state->ns_elapsed_total - prev_ns_elapsed_total) * gb_state->dbg_speed_factor);
    // If the gameboy execution falls behind we don't want it to stay in this loop forever. So we break this loop after
    // 1/60th of a second so that we can atleast update the emulator UI.
    uint64_t loop_timeout = gb_state->ns_elapsed_total + (NS_PER_SEC / 60);
    // See `doc/render.md` for an explanation of this.
    while ((gb_state->ns_elapsed_while_running > (gb_state->m_cycles_elapsed * 954))) {
      if (gb_state->execution_paused) {
        if (gb_state->dbg_step_inst_count == 0) break;
        gb_state->dbg_step_inst_count--;
      }
      if (loop_timeout < SDL_GetTicksNS()) {
        // reset ns_elapsed_while_running to stop the gameboy to run at super speed to catch up once execution speed
        // picks up again.
        gb_state->ns_elapsed_while_running = (gb_state->m_cycles_elapsed * 954);
        break;
      }
      gb_update_io_joyp(gb_state);
      {
        ZoneScopedN("Fetch and Execute");
        if (!gb_state->halted) {
          ZoneTextF("Not Halted");
          uint16_t    prev_pc = gb_state->regs.pc;
          struct inst inst    = fetch(gb_state);
          uint16_t    curr_pc = gb_state->regs.pc;
          if (gb_state->dbg_trace_exec) print_inst(gb_state, gb_state->dbg_trace_exec_fout, inst, true, prev_pc);
          check_breakpoints(gb_state, prev_pc, curr_pc);
          execute(gb_state, inst);
        } else {
          ZoneTextF("Halted");
          // we don't want to stop iterating m cycles while halted or else the timer interrupt will never get called
          gb_state->m_cycles_elapsed++;
        }
      }

      gb_update_timers(gb_state);
      handle_interrupts(gb_state);
      uint8_t curr_mode, last_mode;
      curr_mode = gb_state->regs.io.stat & 0b11;
      last_mode = gb_state->last_mode_handled;

      // TODO: If I want perfect accuracy then I should be copying this incrementally on every iteration for 160
      // m-cycles. I also need to make all memory except hram blocked during this period.

      // TODO: There are some quirks when performing a dma transfer mid line (during OAM_SCAN or DRAWING_PIXELS), i'm
      // currently not sure if this will matter with any real world games so I should look into this.
      if ((curr_mode == OAM_SCAN || curr_mode == DRAWING_PIXELS || curr_mode == VBLANK) && gb_state->oam_dma_start) {
        gb_state->oam_dma_start = false;
        uint8_t oam_dma         = gb_state->regs.io.dma;
        if (oam_dma > 0xDF) {
          oam_dma -= 0x20;
        }
        uint16_t start_src_addr = ((uint16_t)oam_dma) << 8;
        for (uint8_t addr_offset = 0; addr_offset <= 0x9F; addr_offset++) {
          uint16_t src_addr = start_src_addr | addr_offset;
          uint16_t dst_addr = 0xFE00 | addr_offset;
          uint8_t  src_byte = gb_read_mem8(gb_state, src_addr);
          gb_write_mem8(gb_state, dst_addr, src_byte);
        }
      }

      if (!gb_state->headless_mode) {
        ZoneScopedN("Rendering");
        if (curr_mode != last_mode) switch (curr_mode) {
          case OAM_SCAN: {
            ZoneScopedN("OAM Read");
            gb_read_oam_entries(gb_state);
            break;
          }
          case DRAWING_PIXELS: {
            ZoneScopedN("Drawing Pixels");
            gb_draw(gb_state);
            break;
          }
          case HBLANK: {
            ZoneScopedN("H-Blank");
            gb_composite_line(gb_state);
            break;
          }
          case VBLANK: {
            ZoneScopedN("V-Blank");
            gb_flip_frame(gb_state);
            break;
          }
          }
        gb_state->last_mode_handled = curr_mode;
      }
    }
  }

  if (!gb_state->headless_mode) {
    if ((gb_state->regs.io.lcdc & LCDC_ENABLE) == 0) {
      // If screen is disabled we still want to present a blank screen once an iteration so that we can see the imgui
      // UI.
      gb_display_clear(gb_state);
    }
    // If we have fullscreen dockspace enabled then rendering the display to window won't do anything since ImGui will
    // cover it with the dockspace.
    if (!gb_state->enable_fs_dockspace) gb_display_render(gb_state);
    gb_imgui_render(gb_state);
    GB_CheckSDLCall(SDL_RenderPresent(gb_state->sdl_renderer));
  }

  FrameMarkEnd(TracyFrame_SDL_AppIterate);
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  gb_state_t *gb_state = (gb_state_t *)appstate;
  (void)result;
  // This is still called when disassembling where there is no gb_state passed
  // to SDL.
  if (gb_state != NULL) {
    gb_video_free(gb_state);
    gb_state_free(gb_state);
  }
}
