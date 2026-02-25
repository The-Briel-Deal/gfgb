#include "common.h"
#include "cpu.h"
#include "disassemble.h"
#include "ppu.h"
#include "tracy/Tracy.hpp"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL_main.h>

#include <getopt.h>

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

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  enum run_mode run_mode = UNSET;

  int           c;

  // e = execute
  // d = disassemble
  // f: = rom file
  char *rom_filename = NULL;
  // s: = sym file
  char *symbol_filename = NULL;
  // p: = serial port output file
  char *serial_output_filename = NULL;
  // b: = boot rom
  char *bootrom_filename = NULL;
  while ((c = getopt(argc, argv, "edf:s:p:b:")) != -1)
    switch (c) {
    case 'e':
      if (run_mode != UNSET) {
        fprintf(stderr, "Option `e` and `d` specified, these are mutually exclusive\n");
        return SDL_APP_FAILURE;
      }
      run_mode = EXECUTE;
      break;
    case 'd':
      if (run_mode != UNSET) {
        fprintf(stderr, "Option `e` and `d` specified, these are mutually exclusive\n");
        return SDL_APP_FAILURE;
      }
      run_mode = DISASSEMBLE;
      break;
    case 'f': rom_filename = optarg; break;
    case 's': symbol_filename = optarg; break;
    case 'p': serial_output_filename = optarg; break;
    case 'b': bootrom_filename = optarg; break;
    case '?':
      switch (optopt) {
      case 'f':
      case 's':
      case 'p':
      case 'b': fprintf(stderr, "Option -%c requires an argument.\n", optopt); return SDL_APP_FAILURE;
      default:
        if (isprint(optopt)) {
          fprintf(stderr, "Unknown option `-%c'.\n", optopt);
        } else {
          fprintf(stderr, "Unknown option character `0x%.2X'.\n", optopt);
        }
        return SDL_APP_FAILURE;
      }
    default: return SDL_APP_FAILURE;
    }

  switch (run_mode) {
  case EXECUTE: {
    struct gb_state *gb_state = gb_state_alloc();

    *appstate                 = gb_state;
    GB_assert(appstate != NULL);
    gb_state_init((gb_state_t *)*appstate);
    if (!gb_load_rom(gb_state, rom_filename, bootrom_filename, symbol_filename)) return SDL_APP_FAILURE;
    SDL_SetAppMetadata("GF-GB", "0.0.1", "com.gf.gameboy-emu");

    if (serial_output_filename != NULL) {
      gb_state->serial_port_output = fopen(serial_output_filename, "w");
      if (gb_state->serial_port_output == NULL) {
        LogCritical("Error when opening serial port output file: %s", strerror(errno));
        return SDL_APP_FAILURE;
      }
    }

    if (!gb_video_init(gb_state)) return SDL_APP_FAILURE;

    return SDL_APP_CONTINUE; /* carry on with the program! */
  };
  case DISASSEMBLE: {

    struct gb_state gb_state;
    gb_state_init(&gb_state);

    if (!gb_load_rom(&gb_state, rom_filename, bootrom_filename, symbol_filename)) return SDL_APP_FAILURE;
    disassemble(&gb_state, stdout);

    return SDL_APP_SUCCESS;
  }
  case UNSET:
    fprintf(stderr, "Run Mode unset, please specify either `-e` to execute or "
                    "`-d` to disassemble.\n");
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
}

static void check_breakpoints(gb_state_t *gb_state, uint16_t prev_pc, uint16_t curr_pc) {
  for (gb_breakpoint_t bp : gb_state->breakpoints) {
    if (bp.addr >= prev_pc && bp.addr < curr_pc) {
      gb_state->execution_paused = true;
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

  if (!gb_state->execution_paused) {
    for (int i = 0; i < 100; i++) {
      {
        gb_update_io_joyp(gb_state);
        ZoneScopedN("Fetch and Execute");
        if (!gb_state->halted) {
          ZoneTextF("Not Halted");
          uint16_t    prev_pc = gb_state->regs.pc;
          struct inst inst    = fetch(gb_state);
          uint16_t    curr_pc = gb_state->regs.pc;
#ifdef PRINT_INST_DURING_EXEC
          print_inst(gb_state, stdout, inst, true, prev_pc);
#endif
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

      if ((curr_mode == HBLANK || curr_mode == VBLANK) && gb_state->oam_dma_start) {
        gb_state->oam_dma_start = false;
        // TODO: I would love to copy all 0x9F bytes in one memcpy, but unfortunately, not all banks cleanly end on 256
        // byte boundaries. This could be addressed by having `gb_unmap_address()` also return the number of bytes
        // remaining in bank.

        // TODO: Pandocs says that the dma src addr has to be below 0xDF. I need to look into what I should do if it's
        // past this range. For now i'll just put an assert here and fix this problem when/if it causes problems.
        // (https://gbdev.io/pandocs/OAM_DMA_Transfer.html)
        uint8_t oam_dma = gb_state->regs.io.dma;
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
        // TODO: This is not what I should be doing. I should be copying 1 byte for every 1 m-cycle that elapses. While
        // this is being done I should be executing other code but making sure HRAM is the only place that memory can be
        // writen/read.
        gb_state->m_cycles_elapsed += 160;
      }

      {
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
            gb_present(gb_state);
            break;
          }
          }
        gb_state->last_mode_handled = curr_mode;
      }
    }
  }
  if ((gb_state->regs.io.lcdc & LCDC_ENABLE) == 0) {
    // If screen is disabled we still want to present a blank screen once an iteration so that we can see the imgui UI.
    SDL_Surface *locked_texture;
    // Clear the front buffer if the screen is disabled.
    GB_CheckSDLCall(SDL_LockTextureToSurface(gb_state->sdl_composite_target_front, NULL, &locked_texture));
    SDL_ClearSurface(locked_texture, 1.0, 1.0, 1.0, SDL_ALPHA_OPAQUE_FLOAT);
    SDL_UnlockTexture(gb_state->sdl_composite_target_front);

    gb_update_io_joyp(gb_state);
  }
  // We render the front buffer then the imgui UI to make sure we don't display partial frames.
  GB_CheckSDLCall(SDL_SetRenderTarget(gb_state->sdl_renderer, NULL));
  GB_CheckSDLCall(SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT));
  GB_CheckSDLCall(SDL_RenderClear(gb_state->sdl_renderer));
  GB_CheckSDLCall(SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_composite_target_front, NULL, NULL));

  gb_imgui_render(gb_state);
  SDL_RenderPresent(gb_state->sdl_renderer);

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
