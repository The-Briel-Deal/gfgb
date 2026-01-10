#include "common.h"
#include "cpu.h"
#include "disassemble.h"
#include "ppu.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
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

#define GREYSCALE_COLOR(lightness)                                             \
  (SDL_Color) {                                                                \
    .a = 255, .r = 255 * lightness, .g = 255 * lightness,                      \
    .b = 255 * lightness,                                                      \
  }

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
                                  GREYSCALE_COLOR(0.0f / 3),
                                  GREYSCALE_COLOR(1.0f / 3),
                                  GREYSCALE_COLOR(2.0f / 3),
                                  GREYSCALE_COLOR(3.0f / 3),
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

#undef GREYSCALE_COLOR

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

SDL_Texture *get_texture_for_tile(struct gb_state *gb_state,
                                  uint16_t tile_addr) {
  SDL_Renderer *renderer = gb_state->sdl_renderer;
  /* TODO: Use the actual tile data for texture.

  uint8_t *real_address = unmap_address(gb_state, tile_addr);
  */
  SDL_assert(renderer != NULL);
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
                                           SDL_TEXTUREACCESS_STREAMING, 8, 8);
  if (texture == NULL) {
    SDL_Log("SDL_CreateTexture returned null: %s", SDL_GetError());
    abort();
  }
  SDL_SetTexturePalette(texture, gb_state->sdl_palette);

  uint8_t *gb_tile = unmap_address(gb_state, tile_addr);
  uint8_t pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);

  SDL_UpdateTexture(texture, NULL, pixels, 8);
  return texture;
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
  int win_w, win_h;
  SDL_GetWindowSize(gb_state->sdl_window, &win_w, &win_h);
  float w_scale = (float)win_w / GB_DISPLAY_WIDTH;
  float h_scale = (float)win_h / GB_DISPLAY_HEIGHT;

  SDL_Texture *texture = get_texture_for_tile(gb_state, tile_addr);

  bool ret;
  const SDL_FPoint origin_point = {.x = w_scale * x, .y = h_scale * y};
  const SDL_FPoint top_right_point = {.x = origin_point.x + (8.0f * w_scale),
                                      .y = origin_point.y};
  const SDL_FPoint bot_left_point = {.x = origin_point.x,
                                     .y = origin_point.y + (8.0f * h_scale)};
  ret = SDL_RenderTextureAffine(renderer, texture, NULL, &origin_point,
                                &top_right_point, &bot_left_point);
  assert(ret == true);
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

  SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0,
                              SDL_ALPHA_OPAQUE_FLOAT);
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
  if (gb_state->serial_port_output != NULL)
    fclose(gb_state->serial_port_output);
  /* SDL will clean up the window/renderer for us. */
  SDL_free(appstate);
}
