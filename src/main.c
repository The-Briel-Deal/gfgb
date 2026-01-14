#include "common.h"
#include "cpu.h"
#include "disassemble.h"
#include "ppu.h"

#include <SDL3/SDL_init.h>
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

#define GREYSCALE_COLOR(lightness)                                                                                     \
  (SDL_Color) { .a = 255, .r = 255 * lightness, .g = 255 * lightness, .b = 255 * lightness, }

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
  SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, 1600, 1440, SDL_LOGICAL_PRESENTATION_LETTERBOX);
  if (!(gb_state->sdl_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    SDL_Log("Couldn't create palette: %s", SDL_GetError());
    return false;
  }

  if (!SDL_SetPaletteColors(gb_state->sdl_palette,
                            (SDL_Color[4]){
                                GREYSCALE_COLOR(0.0f / 3),
                                GREYSCALE_COLOR(1.0f / 3),
                                GREYSCALE_COLOR(2.0f / 3),
                                GREYSCALE_COLOR(3.0f / 3),
                            },
                            0, DMG_PALETTE_N_COLORS)) {
    SDL_Log("Couldn't set palette colors: %s", SDL_GetError());
    return false;
  }

  SDL_SetDefaultTextureScaleMode(gb_state->sdl_renderer, SDL_SCALEMODE_PIXELART);
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
  SDL_DestroyPalette(gb_state->sdl_palette);
  gb_state->sdl_palette = NULL;
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
    fclose(f);
    memcpy(gb_state->rom0, bytes, bytes_len);
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
    struct gb_state *gb_state;

    gb_state = SDL_malloc(sizeof(struct gb_state));
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
  switch (event->type) {
  case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_RESIZED:
    SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, event->window.data1, event->window.data2,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    break;
  }

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

enum lcdc_flags {
  LCDC_BG_WIN_ENAB = 1 << 0,
  LCDC_OBJ_ENABLE = 1 << 1,
  LCDC_OBJ_SIZE = 1 << 2,
  LCDC_BG_TILE_MAP_AREA = 1 << 3,
  LCDC_BG_WIN_TILE_DATA_AREA = 1 << 4,
  LCDC_WIN_ENABLE = 1 << 5,
  LCDC_WIN_TILEMAP = 1 << 6,
  LCDC_ENABLE = 1 << 7,
};

// TODO: check if tile should be double height (8x16)
void gb_draw_tile(struct gb_state *gb_state, uint8_t x, uint8_t y, uint16_t tile_addr) {
  assert(x < GB_DISPLAY_WIDTH);
  assert(y < GB_DISPLAY_HEIGHT);
  SDL_Renderer *renderer = gb_state->sdl_renderer;
  // TODO: I need to figure out 2 things
  // 1. I need to interleave the two bytes in a gameboy tile before sending it
  // to SDL.
  // 2. I need to figure out a good way to keep track of textures, I could keep
  // one texture for each tile but that seems excessive.
  int win_w, win_h;
  SDL_GetWindowSize(gb_state->sdl_window, &win_w, &win_h);
  float w_scale = (float)win_w / GB_DISPLAY_WIDTH;
  float h_scale = (float)win_h / GB_DISPLAY_HEIGHT;

  SDL_Texture *texture = get_texture_for_tile(gb_state, tile_addr);

  bool ret;
  const SDL_FPoint origin_point = {.x = w_scale * x, .y = h_scale * y};
  const SDL_FPoint top_right_point = {.x = origin_point.x + (8.0f * w_scale), .y = origin_point.y};
  const SDL_FPoint bot_left_point = {.x = origin_point.x, .y = origin_point.y + (8.0f * h_scale)};
  ret = SDL_RenderTextureAffine(renderer, texture, NULL, &origin_point, &top_right_point, &bot_left_point);
  assert(ret == true);
}

void gb_render_bg(struct gb_state *gb_state) {
  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t bg_tile_map_start;
  if (gb_state->regs.io.lcd_control & LCDC_BG_WIN_TILE_DATA_AREA) {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK0_START;
  } else {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK2_START;
  }
  bg_win_tile_data_start_p2 = GB_TILEDATA_BLOCK1_START;

  if (gb_state->regs.io.lcd_control & LCDC_BG_TILE_MAP_AREA) {
    bg_tile_map_start = GB_TILEMAP_BLOCK1_START;
  } else {
    bg_tile_map_start = GB_TILEMAP_BLOCK0_START;
  }

  for (int i = 0; i < (32 * 32); i++) {
    const int x = i % 32;
    const int y = i / 32;
    const uint8_t tile_data_index = read_mem8(gb_state, bg_tile_map_start + i);
    const uint16_t tile_data_addr = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                    ((tile_data_index % 128) * 16);
    // TODO: handle display offset, the display won't always be in the top left.
    uint8_t display_x = x * 8;
    uint8_t display_y = y * 8;
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile(gb_state, display_x, display_y, tile_data_addr);
  }
}

void gb_draw(struct gb_state *gb_state) {
  uint64_t this_frame_ticks_ns = SDL_GetTicksNS();

#ifdef PRINT_FRAME_TIME
  double seconds_since_last_frame = (double)(this_frame_ticks_ns - gb_state->last_frame_ticks_ns) / NS_PER_SEC;
  printf("Frame time = %f seconds\n", seconds_since_last_frame);
#endif

  gb_state->last_frame_ticks_ns = this_frame_ticks_ns;

  SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_RenderClear(gb_state->sdl_renderer);
  gb_render_bg(gb_state);
  SDL_RenderPresent(gb_state->sdl_renderer);
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
  struct gb_state *gb_state = appstate;
#ifdef PRINT_INST_DURING_EXEC
  printf("0x%.4x: ", gb_state->regs.pc);
#endif
  struct inst inst = fetch(gb_state);
#ifdef PRINT_INST_DURING_EXEC
  print_inst(stdout, inst);
#endif
  execute(gb_state, inst);

  // TODO: this doesn't need to be called every iteration.
  gb_draw(gb_state);
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  struct gb_state *gb_state = appstate;
  (void)result;
  // This is still called when disassembling where there is no gb_state passed
  // to SDL.
  if (gb_state != NULL) {
    if (gb_state->serial_port_output != NULL) fclose(gb_state->serial_port_output);

    gb_video_free(gb_state);
    if (gb_state->syms.capacity > 0) {
      free_symbol_list(&gb_state->syms);
    }
    SDL_free(appstate);
  }
}
