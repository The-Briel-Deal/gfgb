#include "ppu.h"
#include "common.h"

#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool gb_video_init(struct gb_state *gb_state) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    LogCritical("Couldn't initialize SDL: %s", SDL_GetError());
    return false;
  }

  if (!SDL_CreateWindowAndRenderer("GF-GB", 1600, 1440, SDL_WINDOW_RESIZABLE, &gb_state->sdl_window,
                                   &gb_state->sdl_renderer)) {
    LogCritical("Couldn't create window/renderer: %s", SDL_GetError());
    return false;
  }
  SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  if (!(gb_state->sdl_bg_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create bg palette: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->sdl_bg_trans0_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create bg palette: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->sdl_obj_palette_0 = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create obj palette 0: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->sdl_obj_palette_1 = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create obj palette 1: %s", SDL_GetError());
    return false;
  }

  SDL_SetDefaultTextureScaleMode(gb_state->sdl_renderer, SDL_SCALEMODE_PIXELART);

  // These targets only need to be 1 line tall since i'm just using these surfaces to store the scanline
  gb_state->sdl_bg_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_INDEX8);
  GF_assert(gb_state->sdl_bg_target != NULL);
  SDL_SetSurfaceBlendMode(gb_state->sdl_bg_target, SDL_BLENDMODE_BLEND);

  // since there are multiple possible palettes objects can use i'm just going to make this surface rgba32. it probably
  // makes it easier when compositing as well since it doesn't need a format change.
  gb_state->sdl_obj_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_RGBA32);
  GF_assert(gb_state->sdl_obj_target != NULL);

  gb_state->sdl_obj_priority_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_RGBA32);
  GF_assert(gb_state->sdl_obj_priority_target != NULL);

  gb_state->sdl_composite_target = SDL_CreateTexture(gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STREAMING, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  GF_assert(gb_state->sdl_composite_target != NULL);

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
  SDL_DestroyPalette(gb_state->sdl_bg_trans0_palette);
  gb_state->sdl_bg_trans0_palette = NULL;
  SDL_DestroyPalette(gb_state->sdl_obj_palette_0);
  gb_state->sdl_obj_palette_0 = NULL;
  SDL_DestroyPalette(gb_state->sdl_obj_palette_1);
  gb_state->sdl_obj_palette_1 = NULL;
  SDL_DestroyRenderer(gb_state->sdl_renderer);
  gb_state->sdl_renderer = NULL;
  SDL_DestroyWindow(gb_state->sdl_window);
  gb_state->sdl_window = NULL;
}

#define PIX(x, y) (((tile_in[(y * 2) + 1] >> (7 - x)) & 1) << 1) | (((tile_in[(y * 2) + 0] >> (7 - x)) & 1) << 0)

//! \brief Convert a gameboy tile from it's byte interleaved 2bit format to 8bit indexed.
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
  if (!SDL_SetPaletteColors(gb_state->sdl_bg_trans0_palette,
                            (SDL_Color[4]){
                                TRANSPARENT_COLOR,
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_1) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_2) / 3.0f),
                                GREYSCALE_COLOR((3.0f - (float)bgp_id_3) / 3.0f),
                            },
                            0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set bg_trans0 palette colors: %s", SDL_GetError());
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

struct oam_entry get_oam_entry(struct gb_state *gb_state, uint8_t index) {
  GF_assert(index < 40);
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
static bool gb_is_tile_in_scanline(struct gb_state *gb_state, int y, int height) {
  uint8_t ly = gb_state->regs.io.ly;
  return ((ly >= y) && (ly <= y + height));
}

// TODO: check if tile should be double height (8x16)
static void gb_draw_tile_to_surface(struct gb_state *gb_state, SDL_Surface *target, SDL_Palette *palette, int x, int y,
                                    uint16_t tile_addr, enum draw_tile_flags flags) {
  GF_assert(x < GB_DISPLAY_WIDTH);
  GF_assert(y < GB_DISPLAY_HEIGHT);
  // TODO: this 8 will need to change to 16 if tile is double height
  if (!gb_is_tile_in_scanline(gb_state, y, 8)) return;

  uint8_t *gb_tile = unmap_address(gb_state, tile_addr);
  uint8_t pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);
  SDL_Surface *tile_surface = SDL_CreateSurfaceFrom(8, 8, SDL_PIXELFORMAT_INDEX8, &pixels, 8);
  SDL_SetSurfacePalette(tile_surface, palette);

  SDL_Rect dstrect = {
      .x = x,
      .y = y - gb_state->regs.io.ly,
      .w = 8,
      .h = 8,
  };

  SDL_FlipMode flip = 0;
  if (flags & DRAW_TILE_FLIP_X) flip |= SDL_FLIP_HORIZONTAL;
  if (flags & DRAW_TILE_FLIP_Y) flip |= SDL_FLIP_VERTICAL;

  if (flip) SDL_FlipSurface(tile_surface, flip);

  SDL_BlitSurface(tile_surface, NULL, target, &dstrect);

  SDL_DestroySurface(tile_surface);
}

static void gb_render_bg(struct gb_state *gb_state, SDL_Surface *target) {
  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t bg_tile_map_start;

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

  SDL_SetSurfacePalette(target, gb_state->sdl_bg_palette);
  for (int i = 0; i < (32 * 32); i++) {
    const int x = i % 32;
    const int y = i / 32;
    const uint8_t tile_data_index = read_mem8(gb_state, bg_tile_map_start + i);
    const uint16_t tile_data_addr = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                    ((tile_data_index % 128) * 16);
    uint8_t display_x = (x * 8) - gb_state->regs.io.scx;
    uint8_t display_y = (y * 8) - gb_state->regs.io.scy;
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile_to_surface(gb_state, target, gb_state->sdl_bg_palette, display_x, display_y, tile_data_addr, 0);
  }
}
static void gb_render_objs(struct gb_state *gb_state, SDL_Surface *target, SDL_Surface *priority_target) {
  bool success;
  success = SDL_ClearSurface(target, 0, 0, 0, 0);
  success = SDL_ClearSurface(priority_target, 0, 0, 0, 0);
  GF_assert(success);
  for (int i = 0; i < 40; i++) {
    struct oam_entry oam_entry = get_oam_entry(gb_state, i);
    enum draw_tile_flags flags = 0;
    if (oam_entry.x_flip) flags |= DRAW_TILE_FLIP_X;
    if (oam_entry.y_flip) flags |= DRAW_TILE_FLIP_Y;
    SDL_Palette *palette;
    if (oam_entry.dmg_palette)
      palette = gb_state->sdl_obj_palette_1;
    else
      palette = gb_state->sdl_obj_palette_0;
    if (oam_entry.priority) {
      gb_draw_tile_to_surface(gb_state, priority_target, palette, oam_entry.x_pos - 8, oam_entry.y_pos - 16,
                              0x8000 + (oam_entry.index * 16), flags);
    } else {
      gb_draw_tile_to_surface(gb_state, target, palette, oam_entry.x_pos - 8, oam_entry.y_pos - 16,
                              0x8000 + (oam_entry.index * 16), flags);
    }
  }
}

void gb_composite_line(struct gb_state *gb_state) {
  bool success;
  SDL_Rect line_rect = {
      .x = 0,
      .y = gb_state->regs.io.ly,
      .h = 1,
      .w = GB_DISPLAY_WIDTH,
  };
  SDL_Surface *locked_texture;
  success = SDL_LockTextureToSurface(gb_state->sdl_composite_target, &line_rect, &locked_texture);
  GF_assert(success);

  // all intermediate targets should have equal dimensions to the locked line (w=GB_DISPLAY_WIDTH h=1)
  GF_assert(locked_texture->h == gb_state->sdl_bg_target->h);
  GF_assert(locked_texture->w == gb_state->sdl_bg_target->w);

  GF_assert(locked_texture->h == gb_state->sdl_obj_priority_target->h);
  GF_assert(locked_texture->w == gb_state->sdl_obj_priority_target->w);

  GF_assert(locked_texture->h == gb_state->sdl_obj_target->h);
  GF_assert(locked_texture->w == gb_state->sdl_obj_target->w);

  SDL_SetSurfacePalette(gb_state->sdl_bg_target, gb_state->sdl_bg_palette);
  SDL_BlitSurface(gb_state->sdl_bg_target, NULL, locked_texture, NULL);

  SDL_BlitSurface(gb_state->sdl_obj_priority_target, NULL, locked_texture, NULL);

  SDL_SetSurfacePalette(gb_state->sdl_bg_target, gb_state->sdl_bg_trans0_palette);
  SDL_BlitSurface(gb_state->sdl_bg_target, NULL, locked_texture, NULL);

  SDL_BlitSurface(gb_state->sdl_obj_target, NULL, locked_texture, NULL);

  SDL_UnlockTexture(gb_state->sdl_composite_target);
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

  TracyCZoneN(update_palettes_ctx, "Palette Update", true);
  update_palettes(gb_state);
  TracyCZoneEnd(update_palettes_ctx);
  TracyCZoneN(render_bg_ctx, "Background Render", true);
  gb_render_bg(gb_state, gb_state->sdl_bg_target);
  TracyCZoneEnd(render_bg_ctx);
  TracyCZoneN(render_objs_ctx, "Object Render", true);
  gb_render_objs(gb_state, gb_state->sdl_obj_target, gb_state->sdl_obj_priority_target);
  TracyCZoneEnd(render_objs_ctx);
}

// called on vblank
void gb_present(struct gb_state *gb_state) {
  bool success;
  /* NULL means that we are selecting the window as the target */
  success = SDL_SetRenderTarget(gb_state->sdl_renderer, NULL);
  GF_assert(success);
  success = SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT);
  GF_assert(success);
  success = SDL_RenderClear(gb_state->sdl_renderer);
  GF_assert(success);
  success = SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_composite_target, NULL, NULL);
  GF_assert(success);
  success = SDL_RenderPresent(gb_state->sdl_renderer);
  GF_assert(success);

  TracyCFrameMark
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
      LogInfo("byte i=%d of indexed_8bit result (%.8b) is not equal to result (%.8b)\n", i, indexed_8bit_tile_result[i],
              indexed_8bit_tile_expect[i]);
      abort();
    }
  }
}
int main() {
  LogInfo("Starting PPU tests.");
  LogInfo("running `test_gb_tile_to_indexed_8bit()`");
  test_gb_tile_to_8bit_indexed();
  LogInfo("PPU tests succeeded.");
  SDL_Quit();
}
#endif
