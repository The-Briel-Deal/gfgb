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

bool gb_video_init(struct gb_state *gb_state) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return false;
  }

  if (!SDL_CreateWindowAndRenderer("examples/renderer/clear", 1600, 1440, SDL_WINDOW_RESIZABLE, &gb_state->sdl_window,
                                   &gb_state->sdl_renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return false;
  }
  SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  if (!(gb_state->sdl_bg_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    SDL_Log("Couldn't create bg palette: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->sdl_obj_palette_0 = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    SDL_Log("Couldn't create obj palette 0: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->sdl_obj_palette_1 = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    SDL_Log("Couldn't create obj palette 1: %s", SDL_GetError());
    return false;
  }

  SDL_SetDefaultTextureScaleMode(gb_state->sdl_renderer, SDL_SCALEMODE_PIXELART);

  gb_state->sdl_bg_target = SDL_CreateTexture(gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                              GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  assert(gb_state->sdl_bg_target != NULL);
  gb_state->sdl_obj_target = SDL_CreateTexture(gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_TARGET,
                                               GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  assert(gb_state->sdl_obj_target != NULL);
  gb_state->sdl_composite_target = SDL_CreateTexture(gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_TARGET, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  assert(gb_state->sdl_composite_target != NULL);

  return true;
}
void gb_video_free(struct gb_state *gb_state) {
  // free all textures
  for (int i = 0; i < DMG_N_TILEDATA_ADDRESSES; i++) {
    if (gb_state->textures[i] != NULL) {
      SDL_DestroyTexture(gb_state->textures[i]);
      gb_state->textures[i] = NULL;
    }
  }
  SDL_DestroyPalette(gb_state->sdl_bg_palette);
  gb_state->sdl_bg_palette = NULL;
  SDL_DestroyPalette(gb_state->sdl_obj_palette_0);
  gb_state->sdl_obj_palette_0 = NULL;
  SDL_DestroyPalette(gb_state->sdl_obj_palette_1);
  gb_state->sdl_obj_palette_1 = NULL;
  SDL_DestroyRenderer(gb_state->sdl_renderer);
  gb_state->sdl_renderer = NULL;
  SDL_DestroyWindow(gb_state->sdl_window);
  gb_state->sdl_window = NULL;
}

#undef GREYSCALE_COLOR

static bool gb_load_rom(struct gb_state *gb_state, const char *rom_name, const char *bootrom_name,
                        const char *sym_name) {
  FILE *f;
  int err;
  uint8_t bytes[KB(16)];
  int bytes_len;

  // Load ROM into gb_state->rom0 (rom is optional since the disassembler can
  // also assemble only the boot rom).
  if (rom_name != NULL) {
    // TODO: Load into multiple banks once bank switching is added.
    f = fopen(rom_name, "r");
    bytes_len = fread(bytes, sizeof(uint8_t), KB(16), f);
    if ((err = ferror(f))) {
      SDL_Log("Error when reading rom file: %d", err);
      return false;
    }
    memcpy(gb_state->rom0, bytes, bytes_len);
    if (!feof(f)) {
      bytes_len = fread(bytes, sizeof(uint8_t), KB(16), f);
      if ((err = ferror(f))) {
        SDL_Log("Error when reading rom file: %d", err);
        return false;
      }
      memcpy(gb_state->rom1, bytes, bytes_len);
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
      SDL_Log("Error when reading symbol file: %d", err);
      return false;
    }
    fclose(f);
  }

  // Load bootrom into gb_state->bootrom (bootrom is optional)
  if (bootrom_name != NULL) {
    f = fopen(bootrom_name, "r");
    bytes_len = fread(gb_state->bootrom, sizeof(uint8_t), 0x0100, f);
    if ((err = ferror(f))) {
      SDL_Log("Error when reading bootrom file: %d", err);
      return false;
    }
    fclose(f);
    assert(bytes_len == 0x0100);
    gb_state->regs.pc = 0x0000;
    gb_state->bootrom_mapped = true;
    int bootrom_name_len = strlen(bootrom_name);
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
      SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Looking for bootrom symbol file at `%s`", bootrom_sym_name);
      f = fopen(bootrom_sym_name, "r");
      if (f == NULL) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                     "Error '%s' occured in when opening symbol file. Is the file present and accessible?",
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
      SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Bootrom filename `%s` does not end with `.gb`", bootrom_name);
    }
  } else {
    gb_state->regs.pc = 0x0100;
    gb_state->bootrom_mapped = false;
  }
  return true;
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  enum run_mode run_mode = UNSET;

  int c;

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

    *appstate = gb_state;
    SDL_assert(appstate != NULL);
    gb_state_init(*appstate);
    if (!gb_load_rom(gb_state, rom_filename, bootrom_filename, symbol_filename)) return SDL_APP_FAILURE;
    SDL_SetAppMetadata("GF-GB", "0.0.1", "com.gf.gameboy-emu");

    if (serial_output_filename != NULL) {
      gb_state->serial_port_output = fopen(serial_output_filename, "w");
      if (gb_state->serial_port_output == NULL) {
        SDL_Log("Error when opening serial port output file: %s", strerror(errno));
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

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  struct gb_state *gb_state = appstate;
  (void)gb_state;
  switch (event->type) {
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
        TracyCZoneTextN(rndr_ctx, "OAM_SCAN");
        TracyCMessageL("OAM_SCAN");
        gb_read_oam_entries(gb_state);
        break;
      case DRAWING_PIXELS:
        TracyCZoneTextN(rndr_ctx, "DRAWING_PIXELS");
        TracyCMessageL("DRAWING_PIXELS");
        gb_draw(gb_state);
        break;
      case HBLANK:
        TracyCZoneTextN(rndr_ctx, "HBLANK");
        TracyCMessageL("HBLANK");
        gb_composite_line(gb_state);
        break;
      case VBLANK:
        TracyCZoneTextN(rndr_ctx, "VBLANK");
        TracyCMessageL("VBLANK");
        gb_present(gb_state);
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
