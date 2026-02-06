#include "ppu.h"
#include "common.h"

#include <SDL3/SDL_rect.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIX(x, y) (((tile_in[(y * 2) + 1] >> (7 - x)) & 1) << 1) | (((tile_in[(y * 2) + 0] >> (7 - x)) & 1) << 0)

//! \brief Convert a gameboy tile to indexed 8bit indexed.
//!
//! The primary objective here is to make it so that I can send gameboy tiles to
//! SDL in a format that SDL understands.
//!
//! For example, `0b1010'1100 0b1100'1011` would turn into
//! `0b11 0b10 0b01 0b00 0b11 0b01 0b10 0b10`
//!
//! Ideally this would convert to MSB2 since every pixel only needs 2 bytes. But due to
//! [this SDL issue](https://github.com/libsdl-org/SDL/issues/14798) this isn't currently supported.
static void gb_tile_to_8bit_indexed(uint8_t *tile_in, uint8_t *tile_out) {
  for (int y = 0; y < 8; y++) {
    uint8_t *line = &tile_out[y * 8];
    for (int x = 0; x < 8; x++) {
      line[x] = PIX(x, y);
    }
  }
}

#undef PIX

inline static uint16_t tile_addr_to_tex_idx(uint16_t tile_addr) {
  // The address this is called with should always cleanly divide by 16.
  assert((tile_addr - GB_TILEDATA_BLOCK0_START) % 16 == 0);
  int tex_index = (tile_addr - GB_TILEDATA_BLOCK0_START) / 16;
  assert(tex_index < DMG_N_TILEDATA_ADDRESSES);
  assert(tex_index >= 0);
  return tex_index;
}

static SDL_Texture *gb_create_tex(struct gb_state *gb_state, uint16_t tile_addr) {
  SDL_Renderer *renderer = gb_state->sdl_renderer;

  uint16_t index = tile_addr_to_tex_idx(tile_addr);

  assert(gb_state->textures[index] == NULL);

  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, 8, 8);
  if (texture == NULL) {
    SDL_Log("SDL_CreateTexture returned null: %s", SDL_GetError());
    abort();
  }
  gb_state->textures[index] = texture;

  return texture;
}

#define GREYSCALE_COLOR(lightness)                                                                                     \
  (SDL_Color) { .a = 255, .r = 255 * lightness, .g = 255 * lightness, .b = 255 * lightness, }
#define TRANSPARENT_COLOR                                                                                              \
  (SDL_Color) { .a = 0, .r = 0, .g = 0, .b = 0, }

static void update_palettes(struct gb_state *gb_state) {
  uint8_t bgp_id_0 = (gb_state->regs.io.bgp >> 0) & 0b11;
  uint8_t bgp_id_1 = (gb_state->regs.io.bgp >> 2) & 0b11;
  uint8_t bgp_id_2 = (gb_state->regs.io.bgp >> 4) & 0b11;
  uint8_t bgp_id_3 = (gb_state->regs.io.bgp >> 6) & 0b11;
  if (!SDL_SetPaletteColors(gb_state->sdl_bg_palette,
                            (SDL_Color[4]){
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_0) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_1) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_2) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_3) / 3.0f),
                            },
                            0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set bg palette colors: %s", SDL_GetError());
  }
  uint8_t obp0_id_1 = (gb_state->regs.io.obp0 >> 2) & 0b11;
  uint8_t obp0_id_2 = (gb_state->regs.io.obp0 >> 4) & 0b11;
  uint8_t obp0_id_3 = (gb_state->regs.io.obp0 >> 6) & 0b11;
  if (!SDL_SetPaletteColors(gb_state->sdl_obj_palette_0,
                            (SDL_Color[4]){
                                TRANSPARENT_COLOR,
                                GREYSCALE_COLOR((3.0f - (float)obp0_id_1) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)obp0_id_2) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)obp0_id_3) / 3.0f),
                            },
                            0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set obj palette 0 colors: %s", SDL_GetError());
  }
  uint8_t obp1_id_1 = (gb_state->regs.io.obp1 >> 2) & 0b11;
  uint8_t obp1_id_2 = (gb_state->regs.io.obp1 >> 4) & 0b11;
  uint8_t obp1_id_3 = (gb_state->regs.io.obp1 >> 6) & 0b11;
  if (!SDL_SetPaletteColors(gb_state->sdl_obj_palette_1,
                            (SDL_Color[4]){
                                TRANSPARENT_COLOR,
                                GREYSCALE_COLOR((3.0f - (float)obp1_id_1) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)obp1_id_2) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)obp1_id_3) / 3.0f),
                            },
                            0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set obj palette 1 colors: %s", SDL_GetError());
  }
}

static SDL_Texture *get_texture_for_tile(struct gb_state *gb_state, uint16_t tile_addr, SDL_Palette *palette) {
  SDL_Texture *texture;

  uint16_t index = tile_addr_to_tex_idx(tile_addr);
  texture = gb_state->textures[index];
  if (texture == NULL) texture = gb_create_tex(gb_state, tile_addr);

  SDL_SetTexturePalette(texture, palette);

  uint8_t *gb_tile = unmap_address(gb_state, tile_addr);
  uint8_t pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);

  SDL_UpdateTexture(texture, NULL, pixels, 8);
  return texture;
}

struct oam_entry get_oam_entry(struct gb_state *gb_state, uint8_t index) {
  assert(index < 40);
  struct oam_entry oam_entry = gb_state->oam_entries[index];

#ifdef DEBUG_PRINT_OAM_ENTRIES
  printf("OAM Entry %d\n", index);
  printf("  x_pos = %d\n", oam_entry.x_pos);
  printf("  y_pos = %d\n", oam_entry.y_pos);
  printf("  bank = %d\n", oam_entry.bank);
  printf("  dmg_palette = %d\n", oam_entry.dmg_palette);
  printf("  index = %d\n", oam_entry.index);
  printf("  priority = %d\n", oam_entry.priority);
  printf("  x_flip = %d\n", oam_entry.x_flip);
  printf("  y_flip = %d\n", oam_entry.y_flip);
#endif

  return oam_entry;
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

enum draw_tile_flags {
  DRAW_TILE_FLIP_X = 1 << 0,
  DRAW_TILE_FLIP_Y = 1 << 1,
  DRAW_TILE_PALETTE_BGP = 1 << 2,
  DRAW_TILE_PALETTE_OBP0 = 1 << 3,
  DRAW_TILE_PALETTE_OBP1 = 1 << 4,
};

// TODO: check if tile should be double height (8x16)
static void gb_draw_tile(struct gb_state *gb_state, int x, int y, uint16_t tile_addr, enum draw_tile_flags flags) {
  assert(x < GB_DISPLAY_WIDTH);
  assert(y < GB_DISPLAY_HEIGHT);
  SDL_Renderer *renderer = gb_state->sdl_renderer;
  int win_w, win_h;
  SDL_GetCurrentRenderOutputSize(gb_state->sdl_renderer, &win_w, &win_h);
  float w_scale = (float)win_w / GB_DISPLAY_WIDTH;
  float h_scale = (float)win_h / GB_DISPLAY_HEIGHT;

  SDL_Palette *palette;
  if (flags & DRAW_TILE_PALETTE_BGP) {
    palette = gb_state->sdl_bg_palette;
  } else if (flags & DRAW_TILE_PALETTE_OBP0) {
    palette = gb_state->sdl_obj_palette_0;
  } else if (flags & DRAW_TILE_PALETTE_OBP1) {
    palette = gb_state->sdl_obj_palette_1;
  } else {
    unreachable();
  }
  SDL_Texture *texture = get_texture_for_tile(gb_state, tile_addr, palette);

  bool ret;
  SDL_FRect dstrect = {
      .x = w_scale * x,
      .y = h_scale * y,
      .w = 8.0f * w_scale,
      .h = 8.0f * h_scale,
  };

  SDL_FlipMode flip = 0;
  if (flags & DRAW_TILE_FLIP_X) flip |= SDL_FLIP_HORIZONTAL;
  if (flags & DRAW_TILE_FLIP_Y) flip |= SDL_FLIP_VERTICAL;

  ret = SDL_RenderTextureRotated(renderer, texture, NULL, &dstrect, 0.0, NULL, flip);
  assert(ret == true);
}

static void gb_render_bg(struct gb_state *gb_state, SDL_Texture *target) {
  bool success;

  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t bg_tile_map_start;

  success = SDL_SetRenderTarget(gb_state->sdl_renderer, target);
  assert(success);
  success = SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT);
  assert(success);
  success = SDL_RenderClear(gb_state->sdl_renderer);
  assert(success);

  if (gb_state->regs.io.lcdc & LCDC_BG_WIN_TILE_DATA_AREA) {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK0_START;
  } else {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK2_START;
  }
  bg_win_tile_data_start_p2 = GB_TILEDATA_BLOCK1_START;

  if (gb_state->regs.io.lcdc & LCDC_BG_TILE_MAP_AREA) {
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
    uint8_t display_x = (x * 8) - gb_state->regs.io.scx;
    uint8_t display_y = (y * 8) - gb_state->regs.io.scy;
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile(gb_state, display_x, display_y, tile_data_addr, DRAW_TILE_PALETTE_BGP);
  }

  assert(success);
}
static void gb_render_objs(struct gb_state *gb_state, SDL_Texture *target) {
  bool success;
  success = SDL_SetRenderTarget(gb_state->sdl_renderer, target);
  assert(success);
  success = SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_TRANSPARENT_FLOAT);
  assert(success);
  success = SDL_RenderClear(gb_state->sdl_renderer);
  assert(success);
  for (int i = 0; i < 40; i++) {
    struct oam_entry oam_entry = get_oam_entry(gb_state, i);
    enum draw_tile_flags flags = 0;
    if (oam_entry.x_flip) flags |= DRAW_TILE_FLIP_X;
    if (oam_entry.y_flip) flags |= DRAW_TILE_FLIP_Y;
    if (oam_entry.dmg_palette)
      flags |= DRAW_TILE_PALETTE_OBP1;
    else
      flags |= DRAW_TILE_PALETTE_OBP0;
    gb_draw_tile(gb_state, oam_entry.x_pos - 8, oam_entry.y_pos - 16, 0x8000 + (oam_entry.index * 16), flags);
  }
}

void gb_composite_line(struct gb_state *gb_state) {
  bool success;
  success = SDL_SetRenderTarget(gb_state->sdl_renderer, gb_state->sdl_composite_target);
  assert(success);
  SDL_FRect line_rect = {
      .x = 0,
      .y = gb_state->regs.io.ly,
      .h = 1,
      .w = GB_DISPLAY_WIDTH,
  };
  success = SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_bg_target, &line_rect, &line_rect);
  assert(success);
  success = SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_obj_target, &line_rect, &line_rect);
  assert(success);
  success = SDL_SetRenderTarget(gb_state->sdl_renderer, NULL);
  assert(success);
}

void gb_read_oam_entries(struct gb_state *gb_state) {
  memcpy(gb_state->oam_entries, gb_state->oam, sizeof(gb_state->oam));
}

void gb_draw(struct gb_state *gb_state) {
  uint64_t this_frame_ticks_ns = SDL_GetTicksNS();

#ifdef PRINT_FRAME_TIME
  double seconds_since_last_frame = (double)(this_frame_ticks_ns - gb_state->last_frame_ticks_ns) / NS_PER_SEC;
  printf("Frame time = %f seconds\n", seconds_since_last_frame);
#endif

  gb_state->last_frame_ticks_ns = this_frame_ticks_ns;

  update_palettes(gb_state);
  gb_render_bg(gb_state, gb_state->sdl_bg_target);
  gb_render_objs(gb_state, gb_state->sdl_obj_target);
}

// called on vblank
void gb_present(struct gb_state *gb_state) {
  bool success;
  /* NULL means that we are selecting the window as the target */
  success = SDL_SetRenderTarget(gb_state->sdl_renderer, NULL);
  assert(success);
  success = SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT);
  assert(success);
  success = SDL_RenderClear(gb_state->sdl_renderer);
  assert(success);
  success = SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_composite_target, NULL, NULL);
  assert(success);
  success = SDL_RenderPresent(gb_state->sdl_renderer);
  assert(success);
}

#ifdef RUN_PPU_TESTS

#include "test_asserts.h"

#include <assert.h>

void test_gb_tile_to_8bit_indexed() {
  uint8_t gb_tile_in[16] = {0};
  uint8_t indexed_8bit_tile_expect[8 * 8] = {0};
  uint8_t indexed_8bit_tile_result[8 * 8] = {0};
  gb_tile_in[0] = 0b1010'1100;
  gb_tile_in[1] = 0b1100'1011;
  indexed_8bit_tile_expect[0] = 0b11;
  indexed_8bit_tile_expect[1] = 0b10;
  indexed_8bit_tile_expect[2] = 0b01;
  indexed_8bit_tile_expect[3] = 0b00;
  indexed_8bit_tile_expect[4] = 0b11;
  indexed_8bit_tile_expect[5] = 0b01;
  indexed_8bit_tile_expect[6] = 0b10;
  indexed_8bit_tile_expect[7] = 0b10;

  gb_tile_to_8bit_indexed(gb_tile_in, indexed_8bit_tile_result);
  for (int i = 0; i < 16; i++) {
    if (indexed_8bit_tile_expect[i] != indexed_8bit_tile_result[i]) {
      SDL_Log("byte i=%d of indexed_8bit result (%.8b) is not equal to result (%.8b)\n", i, indexed_8bit_tile_result[i],
              indexed_8bit_tile_expect[i]);
      abort();
    }
  }
}
int main() {
  SDL_Log("Starting PPU tests.");
  SDL_Log("running `test_gb_tile_to_indexed_8bit()`");
  test_gb_tile_to_8bit_indexed();
  SDL_Log("PPU tests succeeded.");
  SDL_Quit();
}
#endif
