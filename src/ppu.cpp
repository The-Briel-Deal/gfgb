// This is the one CPP file exclusively because of imgui which I didn't want to have to use a wrapper for.

#define GB_LOG_CATEGORY GB_LOG_CATEGORY_PPU
#include "ppu.h"
#include "common.h"

#include <imgui.h>

#include <SDL3_ttf/SDL_ttf.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gb_update_debug_state_text(struct gb_state *gb_state) {
  size_t     fstr_len;
  gb_dstr_t *dstr = &gb_state->debug_state_text;
  gb_dstr_clear(dstr);

  gb_dstr_ensure_space(dstr, 128);
  fstr_len = snprintf(&dstr->txt[dstr->len], 128, "M-Cycles: %ld\n", gb_state->m_cycles_elapsed);
  assert(fstr_len <= 128);
  dstr->len += fstr_len;

  //! If I end up sticking with this approach i'll want to use C++'s format module to format to binary. It looks like
  //! printf("%b") is a gnu extension.
  // gb_dstr_ensure_space(dstr, 128);
  // fstr_len = snprintf(&dstr->txt[dstr->len], 128, "JOYP: 0b%.8b\n", gb_state->regs.io.joyp);
  // assert(fstr_len <= 128);
  // dstr->len += fstr_len;
}

bool gb_video_init(struct gb_state *gb_state) {
  SDL_Environment *env                  = SDL_GetEnvironment();
  const char      *ppu_log_priority_str = SDL_GetEnvironmentVariable(env, "GB_LOG_PPU_PRIORITY");
  if (ppu_log_priority_str != NULL) {
    int ppu_log_priority = atoi(ppu_log_priority_str);
    SDL_SetLogPriority(GB_LOG_CATEGORY_PPU, (SDL_LogPriority)ppu_log_priority);
  }
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
  GB_assert(gb_state->sdl_bg_target != NULL);
  SDL_SetSurfaceBlendMode(gb_state->sdl_bg_target, SDL_BLENDMODE_BLEND);

  gb_state->sdl_win_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_INDEX8);
  GB_assert(gb_state->sdl_win_target != NULL);
  SDL_SetSurfaceBlendMode(gb_state->sdl_win_target, SDL_BLENDMODE_BLEND);

  // since there are multiple possible palettes objects can use i'm just going to make this surface rgba32. it probably
  // makes it easier when compositing as well since it doesn't need a format change.
  gb_state->sdl_obj_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_RGBA32);
  GB_assert(gb_state->sdl_obj_target != NULL);

  gb_state->sdl_obj_priority_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_RGBA32);
  GB_assert(gb_state->sdl_obj_priority_target != NULL);

  gb_state->sdl_composite_target = SDL_CreateTexture(gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32,
                                                     SDL_TEXTUREACCESS_STREAMING, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  GB_assert(gb_state->sdl_composite_target != NULL);

  // Initialize Text Rendering
  bool success = TTF_Init();
  GB_assert(success);

  gb_state->ttf_text_engine = TTF_CreateRendererTextEngine(gb_state->sdl_renderer);
  GB_assert(gb_state->ttf_text_engine != NULL);

  gb_state->ttf_font = TTF_OpenFont("/home/gabe/Downloads/Monocraft-ttf/Monocraft.ttf", 16);
  GB_assert(gb_state->ttf_font != NULL);

  gb_dstr_init(&gb_state->debug_state_text, 8);

  gb_update_debug_state_text(gb_state);

  gb_state->ttf_text = TTF_CreateText(gb_state->ttf_text_engine, gb_state->ttf_font, gb_state->debug_state_text.txt,
                                      gb_state->debug_state_text.len);
  GB_assert(gb_state->ttf_text != NULL);

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
  TTF_DestroyRendererTextEngine(gb_state->ttf_text_engine);
  gb_state->ttf_text_engine = NULL;
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
  {                                                                                                                    \
      .r = (uint8_t)(255 * lightness),                                                                                 \
      .g = (uint8_t)(255 * lightness),                                                                                 \
      .b = (uint8_t)(255 * lightness),                                                                                 \
      .a = 255,                                                                                                        \
  }
#define TRANSPARENT_COLOR                                                                                              \
  {                                                                                                                    \
      .r = 0,                                                                                                          \
      .g = 0,                                                                                                          \
      .b = 0,                                                                                                          \
      .a = 0,                                                                                                          \
  }

static void update_palettes(struct gb_state *gb_state) {
  uint8_t   bgp_id_0      = (gb_state->regs.io.bgp >> 0) & 0b11;
  uint8_t   bgp_id_1      = (gb_state->regs.io.bgp >> 2) & 0b11;
  uint8_t   bgp_id_2      = (gb_state->regs.io.bgp >> 4) & 0b11;
  uint8_t   bgp_id_3      = (gb_state->regs.io.bgp >> 6) & 0b11;
  SDL_Color bgp_colors[4] = {
      GREYSCALE_COLOR((3.0f - (float)bgp_id_0) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->sdl_bg_palette, bgp_colors, 0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set bg palette colors: %s", SDL_GetError());
  }

  SDL_Color bgp_colors_trans[4] = {
      TRANSPARENT_COLOR,
      GREYSCALE_COLOR((3.0f - (float)bgp_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->sdl_bg_trans0_palette, bgp_colors_trans, 0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set bg_trans0 palette colors: %s", SDL_GetError());
  }

  uint8_t   obp0_id_1      = (gb_state->regs.io.obp0 >> 2) & 0b11;
  uint8_t   obp0_id_2      = (gb_state->regs.io.obp0 >> 4) & 0b11;
  uint8_t   obp0_id_3      = (gb_state->regs.io.obp0 >> 6) & 0b11;
  SDL_Color obp0_colors[4] = {
      TRANSPARENT_COLOR,
      GREYSCALE_COLOR((3.0f - (float)obp0_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp0_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp0_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->sdl_obj_palette_0, obp0_colors, 0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set obj palette 0 colors: %s", SDL_GetError());
  }
  uint8_t   obp1_id_1      = (gb_state->regs.io.obp1 >> 2) & 0b11;
  uint8_t   obp1_id_2      = (gb_state->regs.io.obp1 >> 4) & 0b11;
  uint8_t   obp1_id_3      = (gb_state->regs.io.obp1 >> 6) & 0b11;
  SDL_Color obp1_colors[4] = {
      TRANSPARENT_COLOR,
      GREYSCALE_COLOR((3.0f - (float)obp1_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp1_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp1_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->sdl_obj_palette_1, obp1_colors, 0, DMG_PALETTE_N_COLORS)) {
    ERR(gb_state, "Couldn't set obj palette 1 colors: %s", SDL_GetError());
  }
}

const struct oam_entry *get_oam_entry(struct gb_state *gb_state, uint8_t index) {
  GB_assert(index < 40);
  const struct oam_entry *oam_entry = &((struct oam_entry *)gb_state->ram.oam)[index];

#ifdef DEBUG_PRINT_OAM_ENTRIES
  printf("OAM Entry %d\n", index);
  printf("  x_pos = %d\n", oam_entry->x_pos);
  printf("  y_pos = %d\n", oam_entry->y_pos);
  printf("  bank = %d\n", oam_entry->bank);
  printf("  dmg_palette = %d\n", oam_entry->dmg_palette);
  printf("  index = %d\n", oam_entry->index);
  printf("  priority = %d\n", oam_entry->priority);
  printf("  x_flip = %d\n", oam_entry->x_flip);
  printf("  y_flip = %d\n", oam_entry->y_flip);
#endif

  return oam_entry;
}
enum lcdc_flags {
  LCDC_BG_WIN_ENABLE         = 1 << 0,
  LCDC_OBJ_ENABLE            = 1 << 1,
  LCDC_OBJ_SIZE              = 1 << 2,
  LCDC_BG_TILE_MAP_AREA      = 1 << 3,
  LCDC_BG_WIN_TILE_DATA_AREA = 1 << 4,
  LCDC_WIN_ENABLE            = 1 << 5,
  LCDC_WIN_TILEMAP           = 1 << 6,
  LCDC_ENABLE                = 1 << 7,
};

static bool gb_is_tile_in_scanline(struct gb_state *gb_state, int y, int height) {
  uint8_t ly = gb_state->regs.io.ly;
  return ((ly >= y) && (ly <= y + height));
}

// TODO: check if tile should be double height (8x16)
static void gb_draw_tile_to_surface(struct gb_state *gb_state, SDL_Surface *target, SDL_Palette *palette, int x, int y,
                                    uint16_t tile_addr, SDL_FlipMode flip_mode) {
  GB_assert(x < GB_DISPLAY_WIDTH);
  GB_assert(y < GB_DISPLAY_HEIGHT);
  // TODO: this 8 will need to change to 16 if tile is double height
  if (!gb_is_tile_in_scanline(gb_state, y, 8)) return;

  uint8_t *gb_tile = (uint8_t *)unmap_address(gb_state, tile_addr);
  uint8_t  pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);
  SDL_Surface *tile_surface = SDL_CreateSurfaceFrom(8, 8, SDL_PIXELFORMAT_INDEX8, &pixels, 8);
  SDL_SetSurfacePalette(tile_surface, palette);

  SDL_Rect dstrect = {
      .x = x,
      .y = y - gb_state->regs.io.ly,
      .w = 8,
      .h = 8,
  };

  if (flip_mode) SDL_FlipSurface(tile_surface, flip_mode);

  SDL_BlitSurface(tile_surface, NULL, target, &dstrect);

  SDL_DestroySurface(tile_surface);
}

static void gb_render_bg(struct gb_state *gb_state, SDL_Surface *target) {
  bool success;
  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t bg_tile_map_start;
  SDL_SetSurfacePalette(target, gb_state->sdl_bg_palette);
  success = SDL_ClearSurface(target, 1.0f, 1.0f, 1.0f, 1.0f);
  GB_assert(success);
  if (!(gb_state->regs.io.lcdc & LCDC_BG_WIN_ENABLE)) return;

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
    const int      x               = i % 32;
    const int      y               = i / 32;
    const uint8_t  tile_data_index = read_mem8(gb_state, bg_tile_map_start + i);
    const uint16_t tile_data_addr  = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                    ((tile_data_index % 128) * 16);
    uint8_t display_x = (x * 8) - gb_state->regs.io.scx;
    uint8_t display_y = (y * 8) - gb_state->regs.io.scy;
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile_to_surface(gb_state, target, gb_state->sdl_bg_palette, display_x, display_y, tile_data_addr,
                              SDL_FLIP_NONE);
  }
}
static void gb_render_win(struct gb_state *gb_state, SDL_Surface *target) {
  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t win_tile_map_start;

  LogDebug("Window State: line_counter = %d, wx = %d, wy = %d", gb_state->win_line_counter, gb_state->regs.io.wx,
           gb_state->regs.io.wy);

  // Window's X condition in a perfectly accurate pixel FIFO renderer would be checked every pixel, but since my
  // emulator currently uses a scanline based renderer, this should work just as well.
  gb_state->wx_cond = (gb_state->regs.io.wx < 167);

  if ((!gb_state->wy_cond) || (!gb_state->wx_cond) || (!(gb_state->regs.io.lcdc & LCDC_WIN_ENABLE))) {
    gb_state->win_line_blank = true;
    return;
  }
  gb_state->win_line_blank = false;

  if (gb_state->regs.io.lcdc & LCDC_BG_WIN_TILE_DATA_AREA) {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK0_START;
  } else {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK2_START;
  }
  bg_win_tile_data_start_p2 = GB_TILEDATA_BLOCK1_START;

  if (gb_state->regs.io.lcdc & LCDC_WIN_TILEMAP) {
    win_tile_map_start = GB_TILEMAP_BLOCK1_START;
  } else {
    win_tile_map_start = GB_TILEMAP_BLOCK0_START;
  }

  // TODO: Check if this is actually needed, I don't think it is.
  SDL_SetSurfacePalette(target, gb_state->sdl_bg_palette);
  for (int i = 0; i < 32; i++) {
    const int      x               = i;
    const int      y               = gb_state->win_line_counter / 8;
    const uint8_t  tile_data_index = read_mem8(gb_state, win_tile_map_start + x + (y * 32));
    const uint16_t tile_data_addr  = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                    ((tile_data_index % 128) * 16);
    uint8_t display_x = (x * 8) + gb_state->regs.io.wx - 7;
    uint8_t display_y = (gb_state->regs.io.ly / 8) * 8;
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile_to_surface(gb_state, target, gb_state->sdl_bg_palette, display_x, display_y, tile_data_addr,
                              SDL_FLIP_NONE);
  }
  gb_state->win_line_counter++;
}
static void gb_render_objs(struct gb_state *gb_state, SDL_Surface *target, SDL_Surface *priority_target) {
  bool success;
  success = SDL_ClearSurface(target, 0, 0, 0, 0);
  GB_assert(success);
  success = SDL_ClearSurface(priority_target, 0, 0, 0, 0);
  GB_assert(success);
  if (!(gb_state->regs.io.lcdc & LCDC_OBJ_ENABLE)) return;
  // TODO: I need to change this to choose the (up to) ten objects on this line in OAM_SCAN. I'll also want to properly
  // sort objects with equal X positions so that the first one has higher draw priority.
  for (int i = 0; i < 10; i++) {
    uint8_t                 tile_idx;
    const struct oam_entry *oam_entry = gb_state->oam_entries[i];
    if (oam_entry == NULL) break;
    uint32_t flags = 0;
    if (oam_entry->x_flip) flags |= SDL_FLIP_HORIZONTAL;
    if (oam_entry->y_flip) flags |= SDL_FLIP_VERTICAL;
    SDL_Palette *palette;
    if (oam_entry->dmg_palette)
      palette = gb_state->sdl_obj_palette_1;
    else
      palette = gb_state->sdl_obj_palette_0;
    bool draw_double_height = (gb_state->regs.io.lcdc & LCDC_OBJ_SIZE) >> 2;
    tile_idx                = oam_entry->index;
    if (draw_double_height) {
      tile_idx &= 0b1111'1110;
    }
    int x = oam_entry->x_pos - 8;
    int y = oam_entry->y_pos - 16;
    if (oam_entry->y_flip && draw_double_height) {
      y += 8;
    }
  draw_obj:
    if (oam_entry->priority) {
      gb_draw_tile_to_surface(gb_state, priority_target, palette, x, y, 0x8000 + (tile_idx * 16), SDL_FlipMode(flags));
    } else {
      gb_draw_tile_to_surface(gb_state, target, palette, x, y, 0x8000 + (tile_idx * 16), SDL_FlipMode(flags));
    }
    if (draw_double_height) {
      draw_double_height = false;
      tile_idx++;
      if (oam_entry->y_flip) {
        y -= 8;
      } else {
        y += 8;
      }
      goto draw_obj;
    }
  }
}

void gb_composite_line(struct gb_state *gb_state) {
  bool     success;
  SDL_Rect line_rect = {
      .x = 0,
      .y = gb_state->regs.io.ly,
      .w = GB_DISPLAY_WIDTH,
      .h = 1,
  };
  SDL_Surface *locked_texture;
  success = SDL_LockTextureToSurface(gb_state->sdl_composite_target, &line_rect, &locked_texture);
  GB_assert(success);

#ifdef GF_DBG_GREEN_BG
  SDL_ClearSurface(locked_texture, 0, 1, 0, 1);
#endif

  // all intermediate targets should have equal dimensions to the locked line (w=GB_DISPLAY_WIDTH h=1)
  GB_assert(locked_texture->h == gb_state->sdl_bg_target->h);
  GB_assert(locked_texture->w == gb_state->sdl_bg_target->w);

  GB_assert(locked_texture->h == gb_state->sdl_obj_priority_target->h);
  GB_assert(locked_texture->w == gb_state->sdl_obj_priority_target->w);

  GB_assert(locked_texture->h == gb_state->sdl_obj_target->h);
  GB_assert(locked_texture->w == gb_state->sdl_obj_target->w);

#ifndef NDEBUG
  if (!gb_state->dbg_hide_bg)
#endif
  {
    // bg and win use the same palette
    SDL_SetSurfacePalette(gb_state->sdl_bg_target, gb_state->sdl_bg_palette);
    SDL_BlitSurface(gb_state->sdl_bg_target, NULL, locked_texture, NULL);
  }

  // TODO: Adjust width properly
  SDL_Rect win_rect = {
      .x = gb_state->regs.io.wx - 7,
      .y = 0,
      .w = GB_DISPLAY_WIDTH,
      .h = 1,
  };
#ifndef NDEBUG
  if (!gb_state->dbg_hide_win)
#endif
  {
    if (!gb_state->win_line_blank) {
      SDL_SetSurfacePalette(gb_state->sdl_win_target, gb_state->sdl_bg_palette);
      SDL_BlitSurface(gb_state->sdl_win_target, &win_rect, locked_texture, &win_rect);
    }
  }

#ifndef NDEBUG
  if (!gb_state->dbg_hide_objs)
#endif
  {
    SDL_BlitSurface(gb_state->sdl_obj_priority_target, NULL, locked_texture, NULL);
  }

#ifndef NDEBUG
  if (!gb_state->dbg_hide_bg)
#endif
  {
    SDL_SetSurfacePalette(gb_state->sdl_bg_target, gb_state->sdl_bg_trans0_palette);
    SDL_BlitSurface(gb_state->sdl_bg_target, NULL, locked_texture, NULL);
  }

#ifndef NDEBUG
  if (!gb_state->dbg_hide_win)
#endif
  {
    if (!gb_state->win_line_blank) {
      SDL_SetSurfacePalette(gb_state->sdl_win_target, gb_state->sdl_bg_trans0_palette);
      SDL_BlitSurface(gb_state->sdl_win_target, &win_rect, locked_texture, &win_rect);
    }
  }

#ifndef NDEBUG
  if (!gb_state->dbg_hide_objs)
#endif
  {
    SDL_BlitSurface(gb_state->sdl_obj_target, NULL, locked_texture, NULL);
  }

  SDL_UnlockTexture(gb_state->sdl_composite_target);
}

void gb_read_oam_entries(struct gb_state *gb_state) {
  // Window's Y condition is checked for at the start of mode 2 (oam scan) every line.
  if (gb_state->regs.io.ly == gb_state->regs.io.wy) {
    gb_state->wy_cond = true;
  }
  uint8_t oam_entries_pos = 0;
  for (int i = 0; i < 40; i++) {
    if (oam_entries_pos >= 10) {
      break;
    }
    const struct oam_entry *oam_entry          = get_oam_entry(gb_state, i);
    const bool              draw_double_height = (gb_state->regs.io.lcdc & LCDC_OBJ_SIZE) >> 2;
    if (!gb_is_tile_in_scanline(gb_state, oam_entry->y_pos - 16, (draw_double_height) ? 16 : 8)) {
      continue;
    }
    // Sort by x_pos, if x_pos is equal to another entry's x_pos make sure first in oam_mem has highest priority (the
    // final entry in `gb_state->oam_entries` is highest priority since it's the last to be drawn).
    bool inserted = false;
    for (int j = 0; j < oam_entries_pos; j++) {
      if (gb_state->oam_entries[j]->x_pos <= oam_entry->x_pos) {
        memmove(&gb_state->oam_entries[j + 1], &gb_state->oam_entries[j],
                (oam_entries_pos - j) * sizeof(struct oam_entry *));
        gb_state->oam_entries[j] = oam_entry;
        oam_entries_pos++;
        inserted = true;
        break;
      }
    }
    if (!inserted) gb_state->oam_entries[oam_entries_pos++] = oam_entry;
  }
  if (oam_entries_pos < 10) {
    gb_state->oam_entries[oam_entries_pos] = NULL;
  }
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
  TracyCZoneN(render_win_ctx, "Window Render", true);
  gb_render_win(gb_state, gb_state->sdl_win_target);
  TracyCZoneEnd(render_win_ctx);
  TracyCZoneN(render_objs_ctx, "Object Render", true);
  gb_render_objs(gb_state, gb_state->sdl_obj_target, gb_state->sdl_obj_priority_target);
  TracyCZoneEnd(render_objs_ctx);
}

static void gb_draw_dbg_text(struct gb_state *gb_state) {
  GB_CheckSDLCall(SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED));
  GB_CheckSDLCall(TTF_SetTextColor(gb_state->ttf_text, 0, 255, 0, 255));
  GB_CheckSDLCall(
      TTF_SetTextString(gb_state->ttf_text, gb_state->debug_state_text.txt, gb_state->debug_state_text.len));
  GB_CheckSDLCall(TTF_DrawRendererText(gb_state->ttf_text, 0, 0));

  SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

// called on vblank
void gb_present(struct gb_state *gb_state) {
  bool success;

  gb_state->win_line_counter = 0;
  gb_state->wy_cond          = 0;

  /* NULL means that we are selecting the window as the target */
  success = SDL_SetRenderTarget(gb_state->sdl_renderer, NULL);
  GB_assert(success);
  success = SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT);
  GB_assert(success);
  success = SDL_RenderClear(gb_state->sdl_renderer);
  GB_assert(success);
  success = SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_composite_target, NULL, NULL);
  GB_assert(success);

  gb_update_debug_state_text(gb_state);
  gb_draw_dbg_text(gb_state);

  SDL_RenderPresent(gb_state->sdl_renderer);

  TracyCFrameMark
}

#ifdef RUN_PPU_TESTS

#include "test_asserts.h"

#include <assert.h>

void test_gb_tile_to_8bit_indexed() {
  uint8_t gb_tile_in[16]                  = {0};
  uint8_t indexed_8bit_tile_expect[8 * 8] = {0};
  uint8_t indexed_8bit_tile_result[8 * 8] = {0};
  gb_tile_in[0]                           = 0b1010'1100;
  gb_tile_in[1]                           = 0b1100'1011;
  indexed_8bit_tile_expect[0]             = 0b11;
  indexed_8bit_tile_expect[1]             = 0b10;
  indexed_8bit_tile_expect[2]             = 0b01;
  indexed_8bit_tile_expect[3]             = 0b00;
  indexed_8bit_tile_expect[4]             = 0b11;
  indexed_8bit_tile_expect[5]             = 0b01;
  indexed_8bit_tile_expect[6]             = 0b10;
  indexed_8bit_tile_expect[7]             = 0b10;

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
