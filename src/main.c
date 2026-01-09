#include "common.h"
#include "cpu.h"
#include "disassemble.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL_main.h>

#include <getopt.h>

enum run_mode {
  UNSET = 0,
  EXECUTE,
  DISASSEMBLE,
};

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  enum run_mode run_mode = UNSET;

  int c;
  char *filename = NULL;
  char *symbol_filename = NULL;
  char *serial_output_filename = NULL;

  // e = execute
  // d = disassemble
  // f: = rom file
  // s: = sym file
  // p: = serial port output file
  while ((c = getopt(argc, argv, "edf:s:p:")) != -1)
    switch (c) {
    case 'e':
      if (run_mode != UNSET) {
        fprintf(stderr,
                "Option `e` and `d` specified, these are mutually exclusive\n");
        return 1;
      }
      run_mode = EXECUTE;
      break;
    case 'd':
      if (run_mode != UNSET) {
        fprintf(stderr,
                "Option `e` and `d` specified, these are mutually exclusive\n");
        return 1;
      }
      run_mode = DISASSEMBLE;
      break;
    case 'f': filename = optarg; break;
    case 's': symbol_filename = optarg; break;
    case 'p': serial_output_filename = optarg; break;
    case '?':
      if (optopt == 'f')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      if (optopt == 's')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default: abort();
    }

  switch (run_mode) {
  case EXECUTE: {
    FILE *f;
    int err;
    uint8_t bytes[KB(16)];
    int bytes_len;
    struct gb_state *gb_state;

    f = fopen(filename, "r");

    bytes_len = fread(bytes, sizeof(uint8_t), KB(16), f);
    if ((err = ferror(f))) {
      SDL_Log("Error when reading rom file: %d", err);
      return SDL_APP_FAILURE;
    }
    fclose(f);

    gb_state = SDL_malloc(sizeof(struct gb_state));
    *appstate = gb_state;
    SDL_assert(appstate != NULL);
    gb_state_init(*appstate);
    SDL_SetAppMetadata("GF-GB", "0.0.1", "com.gf.gameboy-emu");
    memcpy(gb_state->rom0, bytes, bytes_len);

    if (serial_output_filename != NULL) {
      gb_state->serial_port_output = fopen(serial_output_filename, "w");
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
      SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer(
            "examples/renderer/clear", 1600, 1440, SDL_WINDOW_RESIZABLE,
            &gb_state->sdl_window, &gb_state->sdl_renderer)) {
      SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, 1600, 1440,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if (!(gb_state->sdl_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
      SDL_Log("Couldn't create palette: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    if (!SDL_SetPaletteColors(gb_state->sdl_palette,
                              (SDL_Color[4]){
                                  (SDL_Color){
                                      .a = 255,
                                      .r = 255 * (3 / 3),
                                      .g = 255 * (3 / 3),
                                      .b = 255 * (3 / 3),
                                  },
                                  (SDL_Color){
                                      .a = 255,
                                      .r = 255 * (2 / 3),
                                      .g = 255 * (2 / 3),
                                      .b = 255 * (2 / 3),
                                  },
                                  (SDL_Color){
                                      .a = 255,
                                      .r = 255 * (1 / 3),
                                      .g = 255 * (1 / 3),
                                      .b = 255 * (1 / 3),
                                  },
                                  (SDL_Color){
                                      .a = 255,
                                      .r = 255 * (0 / 3),
                                      .g = 255 * (0 / 3),
                                      .b = 255 * (0 / 3),
                                  },
                              },
                              0, DMG_PALETTE_N_COLORS)) {
      SDL_Log("Couldn't set palette colors: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE; /* carry on with the program! */
  };
  case DISASSEMBLE: {
    FILE *f;
    int err;

    f = fopen(filename, "r");
    uint8_t bytes[KB(16)];

    int len = fread(bytes, sizeof(uint8_t), KB(16), f);
    if ((err = ferror(f))) {
      SDL_Log("Error when reading rom file: %d", err);
      return SDL_APP_FAILURE;
    }
    fclose(f);

    if (symbol_filename != NULL) {
      struct debug_symbol_list syms;
      alloc_symbol_list(&syms);
      f = fopen(symbol_filename, "r");
      parse_syms(&syms, f);
      if ((err = ferror(f))) {
        SDL_Log("Error when reading symbol file: %d", err);
        return SDL_APP_FAILURE;
      }
      fclose(f);
      disassemble_rom_with_sym(stdout, bytes, len, &syms);
      free_symbol_list(&syms);
    } else {
      disassemble_rom(stdout, bytes, len);
    }

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
    SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer,
                                     event->window.data1, event->window.data2,
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

void get_texture_for_tile(struct gb_state *gb_state, uint16_t tile_addr) {
  SDL_Renderer *renderer = gb_state->sdl_renderer;
  /* TODO: Use the actual tile data for texture.

  uint8_t *real_address = unmap_address(gb_state, tile_addr);
  */
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX2MSB,
                                           SDL_TEXTUREACCESS_STATIC, 8, 8);
  assert(texture != NULL);
  SDL_SetTexturePalette(texture, gb_state->sdl_palette);

  uint8_t *gb_tile = unmap_address(gb_state, tile_addr);
  uint8_t pixels[16] = {0};
  for (int i = 0; i < 8; i++) {
    uint8_t b1 = gb_tile[(i * 2) + 0];
    uint8_t b2 = gb_tile[(i * 2) + 1];
    for (int j = 0; j < 8; j++) {
      pixels[(i * 2) + (j / 4)] |= ((1 << j) & b1);
      pixels[(i * 2) + (j / 4)] |= (((1 << j) & b2) >> 1);
    }
  }

  SDL_UpdateTexture(texture, NULL, pixels, 2);
}

// TODO: check if tile should be double height (8x16)
void gb_draw_tile(struct gb_state *gb_state, uint8_t x, uint8_t y,
                  uint16_t tile_addr) {
  assert(x < GB_DISPLAY_WIDTH);
  assert(y < GB_DISPLAY_HEIGHT);
  SDL_Renderer *renderer = gb_state->sdl_renderer;
  // TODO: I need to figure out 2 things
  // 1. I need to interleave the two bytes in a gameboy tile before sending it
  // to SDL.
  // 2. I need to figure out a good way to keep track of textures, I could keep
  // one texture for each tile but that seems excessive.
  // SDL_UpdateTexture(SDL_Texture *texture, const SDL_Rect *rect, const void
  // *pixels, int pitch)
}

void gb_render_bg(struct gb_state *gb_state) {
  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t bg_tile_map_start;
  if (gb_state->regs.io.lcd_control & LCDC_BG_WIN_TILE_DATA_AREA) {
    bg_win_tile_data_start_p1 = 0x8000;
  } else {
    bg_win_tile_data_start_p1 = 0x9000;
  }
  // Tile data 128-256 is always in Block 1 (0x8800-0x8FFF)
  bg_win_tile_data_start_p2 = 0x8800;
  if (gb_state->regs.io.lcd_control & LCDC_BG_TILE_MAP_AREA) {
    bg_tile_map_start = 0x9C00;
  } else {
    bg_tile_map_start = 0x9800;
  }

  for (int i = 0; i < (32 * 32); i++) {
    const int x = i % 32;
    const int y = i / 32;
    const uint8_t tile_data_index = read_mem8(gb_state, bg_tile_map_start + i);
    const uint16_t tile_data_addr =
        (tile_data_index < 128 ? bg_win_tile_data_start_p1
                               : bg_win_tile_data_start_p2) +
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
  double seconds_since_last_frame =
      (double)(this_frame_ticks_ns - gb_state->last_frame_ticks_ns) /
      NS_PER_SEC;
  printf("Frame time = %f seconds\n", seconds_since_last_frame);
#endif

  gb_state->last_frame_ticks_ns = this_frame_ticks_ns;

  gb_render_bg(gb_state);
  SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0,
                              SDL_ALPHA_OPAQUE_FLOAT);

  /* clear the window to the draw color. */
  SDL_RenderClear(gb_state->sdl_renderer);
  for (int y = 0; y < GB_DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < GB_DISPLAY_WIDTH; x++) {
      uint8_t pixel = gb_state->display[y][x];
      // The original gameboy had 4 shades of grey, these are represented by
      // 0,1,2,3. Anything greater is invalid.
      SDL_assert(pixel < 4);

      float grey_shade = (float)pixel / 3.0;

      SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, grey_shade,
                                  grey_shade, grey_shade,
                                  SDL_ALPHA_OPAQUE_FLOAT);
      int w, h;
      SDL_GetRenderLogicalPresentation(gb_state->sdl_renderer, &w, &h, NULL);
      float pixel_w = (float)w / (float)GB_DISPLAY_WIDTH;
      float pixel_h = (float)h / (float)GB_DISPLAY_HEIGHT;

      struct SDL_FRect rect = {
          .x = pixel_w * x, .y = pixel_h * y, .w = pixel_w, .h = pixel_h};
      SDL_RenderFillRect(gb_state->sdl_renderer, &rect);
    }
  }
  /* put the newly-cleared rendering on the screen. */
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
  if (gb_state->serial_port_output != NULL)
    fclose(gb_state->serial_port_output);
  /* SDL will clean up the window/renderer for us. */
  SDL_free(appstate);
}
