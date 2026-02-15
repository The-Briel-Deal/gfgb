#include "common.h"
#include "cpu.h"
#include "disassemble.h"
#include "ppu.h"
#include "tracy/TracyC.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
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

  // Load bootrom into gb_state->bootrom (bootrom is optional)
  if (bootrom_name != NULL) {
    f         = fopen(bootrom_name, "r");
    bytes_len = fread(gb_state->ram.bootrom, sizeof(uint8_t), 0x0100, f);
    if ((err = ferror(f))) {
      LogCritical("Error when reading bootrom file: %d", err);
      return false;
    }
    fclose(f);
    GB_assert(bytes_len == 0x0100);
    gb_state->regs.pc        = 0x0000;
    gb_state->bootrom_mapped = true;
    int bootrom_name_len     = strlen(bootrom_name);
    // TODO: Handle case where bootrom name ends with `.bin`
    // TODO: Identifying if a bootrom sym file is present should be moved to a helper fn
    if (memcmp(&bootrom_name[bootrom_name_len - 3], ".gb", 3) == 0) {
      // I need a string that is two more chars long since `.sym` is a character longer than `.gb`, and we also need
      // room for a null term.
      char *bootrom_sym_name = malloc(bootrom_name_len + 2);

      strcpy(bootrom_sym_name, bootrom_name);

      bootrom_sym_name[bootrom_name_len - 2] = 's';
      bootrom_sym_name[bootrom_name_len - 1] = 'y';
      bootrom_sym_name[bootrom_name_len - 0] = 'm';
      bootrom_sym_name[bootrom_name_len + 1] = '\0';
      LogCritical("Looking for bootrom symbol file at `%s`", bootrom_sym_name);
      f = fopen(bootrom_sym_name, "r");
      if (f == NULL) {
        LogDebug("Error '%s' occured in when opening symbol file. Is the file present and accessible?",
                 strerror(errno));
      }

      if (f != NULL) {
        parse_syms(&gb_state->syms, f);
        if (ferror(f) == 0) {
          gb_state->bootrom_has_syms = true;
        }
        fclose(f);
      }
      free(bootrom_sym_name);
    } else {
      LogDebug("Bootrom filename `%s` does not end with `.gb`", bootrom_name);
    }
  } else {
    gb_state->regs.pc        = 0x0100;
    gb_state->bootrom_mapped = false;
  }
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
        return 1;
      }
      run_mode = EXECUTE;
      break;
    case 'd':
      if (run_mode != UNSET) {
        fprintf(stderr, "Option `e` and `d` specified, these are mutually exclusive\n");
        return 1;
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
    gb_state_init(*appstate);
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
  case SDL_EVENT_KEY_UP: break;
  case SDL_EVENT_KEY_DOWN: {
    switch (event->key) {
    case SDLK_1: {
#ifndef NDEBUG
      gb_state->dbg_hide_bg = !gb_state->dbg_hide_bg;
#endif
      break;
    }
    case SDLK_2: {
#ifndef NDEBUG
      gb_state->dbg_hide_win = !gb_state->dbg_hide_win;
#endif
      break;
    }
    case SDLK_3: {
#ifndef NDEBUG
      gb_state->dbg_hide_objs = !gb_state->dbg_hide_objs;
#endif
      break;
    }
    }
    break;
  }
  default: unreachable();
  }
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  struct gb_state *gb_state = appstate;
  gb_video_handle_sdl_event(gb_state, event);
  switch (event->type) {
  case SDL_EVENT_KEY_UP:
  case SDL_EVENT_KEY_DOWN: handle_key_event(gb_state, &event->key); break;
  case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_RESIZED: /* no action should be needed since the the logical representation is the gb width x
                                    height, screen will be automatically letter boxed on resize */
    break;
  }

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

char *get_inst_symbol(struct gb_state *gb_state) {
  // This works because we know the symbols are sorted.
  uint16_t pc = gb_state->regs.pc;
  for (int i = 0; i < gb_state->syms.len; i++) {
    struct debug_symbol *sym = &gb_state->syms.syms[i];
    if (sym->start_offset <= pc && pc < sym->len + sym->start_offset) {
      return sym->name;
    }
  }
  return "Unknown";
}

#define TracyCZoneTextN(ctx, txt) TracyCZoneText(ctx, txt, sizeof(txt));

const char *const TracyFrame_SDL_AppIterate = "App Iteration";

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
  TracyCFrameMarkStart(TracyFrame_SDL_AppIterate);
  struct gb_state *gb_state = appstate;

  for (int i = 0; i < 100; i++) {
    TracyCZoneN(ctx, "Fetch and Execute", true);
    if (!gb_state->halted) {
      TracyCZoneColor(ctx, TRACY_COLOR_GREEN);
      TracyCZoneTextN(ctx, "Not Halted");
#ifdef PRINT_INST_DURING_EXEC
      printf("%s:0x%.4x: ", get_inst_symbol(gb_state), gb_state->regs.pc);
#endif
      struct inst inst = fetch(gb_state);
      execute(gb_state, inst);
    } else {
      // Halted
      TracyCZoneColor(ctx, TRACY_COLOR_RED);
      TracyCZoneTextN(ctx, "Halted");
      // we don't want to stop iterating m cycles while halted or else the timer interrupt will never get called
      gb_state->m_cycles_elapsed++;
    }
    TracyCZoneEnd(ctx);

    update_timers(gb_state);
    handle_interrupts(gb_state);
    uint8_t curr_mode, last_mode;
    curr_mode = gb_state->regs.io.stat & 0b11;
    last_mode = gb_state->last_mode_handled;

    TracyCZoneN(rndr_ctx, "Rendering", true);
    if (curr_mode != last_mode) switch (curr_mode) {
      case OAM_SCAN:
        TracyCZoneN(oam_ctx, "OAM Read", true);
        gb_read_oam_entries(gb_state);
        TracyCZoneEnd(oam_ctx);
        break;
      case DRAWING_PIXELS:
        TracyCZoneN(draw_pix_ctx, "Drawing Pixels", true);
        gb_draw(gb_state);
        TracyCZoneEnd(draw_pix_ctx);
        break;
      case HBLANK:
        TracyCZoneN(hblank_ctx, "H-Blank", true);
        gb_composite_line(gb_state);
        TracyCZoneEnd(hblank_ctx);
        break;
      case VBLANK:
        TracyCZoneN(vblank_ctx, "V-Blank", true);
        gb_present(gb_state);
        TracyCZoneEnd(vblank_ctx);
        break;
      }
    gb_state->last_mode_handled = curr_mode;
    TracyCZoneEnd(rndr_ctx);
  }

  TracyCFrameMarkEnd(TracyFrame_SDL_AppIterate);
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  struct gb_state *gb_state = appstate;
  (void)result;
  // This is still called when disassembling where there is no gb_state passed
  // to SDL.
  if (gb_state != NULL) {
    gb_video_free(gb_state);
    gb_state_free(gb_state);
  }
}
