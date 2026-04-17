#include "gui.h"
#define GB_LOG_CATEGORY GB_LOG_CATEGORY_PPU
#include "common.h"

#include "disassemble.h"
#include "ppu.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  const char *video_driver = SDL_GetCurrentVideoDriver();
  LogInfo("SDL Video Driver: %s", video_driver);

  if (memcmp(video_driver, "dummy", 5) == 0) {
    gb_state->dbg.headless_mode = true;
  }

  if (!SDL_CreateWindowAndRenderer("GF-GB", 1600, 1440, SDL_WINDOW_RESIZABLE, &gb_state->video.sdl_window,
                                   &gb_state->video.sdl_renderer)) {
    LogCritical("Couldn't create window/renderer: %s", SDL_GetError());
    return false;
  }
  SDL_SetRenderLogicalPresentation(gb_state->video.sdl_renderer, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);
  if (!(gb_state->video.sdl_bg_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create bg palette: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->video.sdl_bg_trans0_palette = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create bg palette: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->video.sdl_obj_palette_0 = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create obj palette 0: %s", SDL_GetError());
    return false;
  }
  if (!(gb_state->video.sdl_obj_palette_1 = SDL_CreatePalette(DMG_PALETTE_N_COLORS))) {
    LogCritical("Couldn't create obj palette 1: %s", SDL_GetError());
    return false;
  }

  SDL_SetDefaultTextureScaleMode(gb_state->video.sdl_renderer, SDL_SCALEMODE_PIXELART);

  // These targets only need to be 1 line tall since i'm just using these surfaces to store the scanline
  gb_state->video.sdl_bg_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_INDEX8);
  GB_assert(gb_state->video.sdl_bg_target != NULL);
  SDL_SetSurfaceBlendMode(gb_state->video.sdl_bg_target, SDL_BLENDMODE_BLEND);

  gb_state->video.sdl_win_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_INDEX8);
  GB_assert(gb_state->video.sdl_win_target != NULL);
  SDL_SetSurfaceBlendMode(gb_state->video.sdl_win_target, SDL_BLENDMODE_BLEND);

  // since there are multiple possible palettes objects can use i'm just going to make this surface rgba32. it probably
  // makes it easier when compositing as well since it doesn't need a format change.
  gb_state->video.sdl_obj_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_RGBA32);
  GB_assert(gb_state->video.sdl_obj_target != NULL);

  gb_state->video.sdl_obj_priority_target = SDL_CreateSurface(GB_DISPLAY_WIDTH, 1, SDL_PIXELFORMAT_RGBA32);
  GB_assert(gb_state->video.sdl_obj_priority_target != NULL);

  gb_state->video.sdl_composite_target_front =
      SDL_CreateTexture(gb_state->video.sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                        GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  GB_assert(gb_state->video.sdl_composite_target_front != NULL);
  gb_state->video.sdl_composite_target_back =
      SDL_CreateTexture(gb_state->video.sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING,
                        GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  GB_assert(gb_state->video.sdl_composite_target_back != NULL);

  gb_imgui_init(gb_state);

  gb_state->video.initialized = true;

  return true;
}

void gb_video_free(struct gb_state *gb_state) {
  if (!gb_state->video.initialized) return;
  // free all textures
  for (int i = 0; i < DMG_N_TILEDATA_ADDRESSES; i++) {
    if (gb_state->video.textures[i] != NULL) {
      SDL_DestroyTexture(gb_state->video.textures[i]);
      gb_state->video.textures[i] = NULL;
    }
  }
  gb_imgui_free(gb_state);

  SDL_DestroyPalette(gb_state->video.sdl_bg_palette);
  gb_state->video.sdl_bg_palette = NULL;
  SDL_DestroyPalette(gb_state->video.sdl_bg_trans0_palette);
  gb_state->video.sdl_bg_trans0_palette = NULL;
  SDL_DestroyPalette(gb_state->video.sdl_obj_palette_0);
  gb_state->video.sdl_obj_palette_0 = NULL;
  SDL_DestroyPalette(gb_state->video.sdl_obj_palette_1);
  gb_state->video.sdl_obj_palette_1 = NULL;
  SDL_DestroySurface(gb_state->video.sdl_bg_target);
  gb_state->video.sdl_bg_target = NULL;
  SDL_DestroySurface(gb_state->video.sdl_win_target);
  gb_state->video.sdl_win_target = NULL;
  SDL_DestroySurface(gb_state->video.sdl_obj_target);
  gb_state->video.sdl_obj_target = NULL;
  SDL_DestroySurface(gb_state->video.sdl_obj_priority_target);
  gb_state->video.sdl_obj_priority_target = NULL;
  SDL_DestroyTexture(gb_state->video.sdl_composite_target_front);
  gb_state->video.sdl_composite_target_front = NULL;
  SDL_DestroyTexture(gb_state->video.sdl_composite_target_back);
  gb_state->video.sdl_composite_target_back = NULL;
  SDL_DestroyRenderer(gb_state->video.sdl_renderer);
  gb_state->video.sdl_renderer = NULL;
  SDL_DestroyWindow(gb_state->video.sdl_window);
  gb_state->video.sdl_window = NULL;
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
void gb_tile_to_8bit_indexed(uint8_t *tile_in, uint8_t *tile_out) {
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
  uint8_t   bgp_id_0      = (gb_state->saved.regs.io.bgp >> 0) & 0b11;
  uint8_t   bgp_id_1      = (gb_state->saved.regs.io.bgp >> 2) & 0b11;
  uint8_t   bgp_id_2      = (gb_state->saved.regs.io.bgp >> 4) & 0b11;
  uint8_t   bgp_id_3      = (gb_state->saved.regs.io.bgp >> 6) & 0b11;
  SDL_Color bgp_colors[4] = {
      GREYSCALE_COLOR((3.0f - (float)bgp_id_0) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->video.sdl_bg_palette, bgp_colors, 0, DMG_PALETTE_N_COLORS)) {
    Err(gb_state, "Couldn't set bg palette colors: %s", SDL_GetError());
  }

  SDL_Color bgp_colors_trans[4] = {
      TRANSPARENT_COLOR,
      GREYSCALE_COLOR((3.0f - (float)bgp_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)bgp_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->video.sdl_bg_trans0_palette, bgp_colors_trans, 0, DMG_PALETTE_N_COLORS)) {
    Err(gb_state, "Couldn't set bg_trans0 palette colors: %s", SDL_GetError());
  }

  uint8_t   obp0_id_1      = (gb_state->saved.regs.io.obp0 >> 2) & 0b11;
  uint8_t   obp0_id_2      = (gb_state->saved.regs.io.obp0 >> 4) & 0b11;
  uint8_t   obp0_id_3      = (gb_state->saved.regs.io.obp0 >> 6) & 0b11;
  SDL_Color obp0_colors[4] = {
      TRANSPARENT_COLOR,
      GREYSCALE_COLOR((3.0f - (float)obp0_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp0_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp0_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->video.sdl_obj_palette_0, obp0_colors, 0, DMG_PALETTE_N_COLORS)) {
    Err(gb_state, "Couldn't set obj palette 0 colors: %s", SDL_GetError());
  }
  uint8_t   obp1_id_1      = (gb_state->saved.regs.io.obp1 >> 2) & 0b11;
  uint8_t   obp1_id_2      = (gb_state->saved.regs.io.obp1 >> 4) & 0b11;
  uint8_t   obp1_id_3      = (gb_state->saved.regs.io.obp1 >> 6) & 0b11;
  SDL_Color obp1_colors[4] = {
      TRANSPARENT_COLOR,
      GREYSCALE_COLOR((3.0f - (float)obp1_id_1) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp1_id_2) / 3.0f),
      GREYSCALE_COLOR((3.0f - (float)obp1_id_3) / 3.0f),
  };
  if (!SDL_SetPaletteColors(gb_state->video.sdl_obj_palette_1, obp1_colors, 0, DMG_PALETTE_N_COLORS)) {
    Err(gb_state, "Couldn't set obj palette 1 colors: %s", SDL_GetError());
  }
}

const struct oam_entry *get_oam_entry(struct gb_state *gb_state, uint8_t index) {
  GB_assert(index < 40);
  const struct oam_entry *oam_entry = &((struct oam_entry *)gb_state->saved.mem.oam)[index];

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

static bool gb_is_tile_in_scanline(struct gb_state *gb_state, int y, int height) {
  uint8_t ly = gb_state->saved.regs.io.ly;
  return ((ly >= y) && (ly < y + height));
}

static void gb_draw_tile_to_line(struct gb_state *gb_state, SDL_Surface *target, SDL_Palette *palette, int x, int y,
                                 uint16_t tile_addr, SDL_FlipMode flip_mode) {
  GB_assert(x < GB_DISPLAY_WIDTH);
  GB_assert(y < GB_DISPLAY_HEIGHT);
  if (!gb_is_tile_in_scanline(gb_state, y, 8)) return;

  gb_draw_tile_to_surface(gb_state, target, palette, x, y - gb_state->saved.regs.io.ly, tile_addr, flip_mode);
}

void gb_draw_tile_to_surface(struct gb_state *gb_state, SDL_Surface *target, SDL_Palette *palette, int x, int y,
                             uint16_t tile_addr, SDL_FlipMode flip_mode) {

  uint8_t *gb_tile = (uint8_t *)gb_unmap_address(gb_state, tile_addr);
  uint8_t  pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);
  SDL_Surface *tile_surface = SDL_CreateSurfaceFrom(8, 8, SDL_PIXELFORMAT_INDEX8, &pixels, 8);
  SDL_SetSurfacePalette(tile_surface, palette);
  SDL_SetSurfaceBlendMode(tile_surface, SDL_BLENDMODE_BLEND);

  SDL_Rect dstrect = {
      .x = x,
      .y = y,
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
  SDL_SetSurfacePalette(target, gb_state->video.sdl_bg_palette);
  success = SDL_ClearSurface(target, 1.0f, 1.0f, 1.0f, 1.0f);
  GB_assert(success);
  if (!(gb_state->saved.regs.io.lcdc & LCDC_BG_WIN_ENABLE)) return;

  if (gb_state->saved.regs.io.lcdc & LCDC_BG_WIN_TILE_DATA_AREA) {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK0_START;
  } else {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK2_START;
  }
  bg_win_tile_data_start_p2 = GB_TILEDATA_BLOCK1_START;

  if (gb_state->saved.regs.io.lcdc & LCDC_BG_TILE_MAP_AREA) {
    bg_tile_map_start = GB_TILEMAP_BLOCK1_START;
  } else {
    bg_tile_map_start = GB_TILEMAP_BLOCK0_START;
  }

  for (int i = 0; i < (32 * 32); i++) {
    const int      x               = i % 32;
    const int      y               = i / 32;
    const uint8_t  tile_data_index = gb_read_mem(gb_state, bg_tile_map_start + i);
    const uint16_t tile_data_addr  = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                     ((tile_data_index % 128) * 16);
    // Use signed integers when calculating the display position since we still display tiles where y = 0 to -7
    int16_t display_x = (x * 8) - gb_state->saved.regs.io.scx;
    int16_t display_y = (y * 8) - gb_state->saved.regs.io.scy;
    if (display_x < -7) display_x += 256;
    if (display_y < -7) display_y += 256;
    GB_assert(display_x > -8 && display_x < 256);
    GB_assert(display_y > -8 && display_y < 256);
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile_to_line(gb_state, target, gb_state->video.sdl_bg_palette, display_x, display_y, tile_data_addr,
                           SDL_FLIP_NONE);
  }
}
static void gb_render_win(struct gb_state *gb_state, SDL_Surface *target) {
  // TODO: Make sure screen and bg are enabled before this
  uint16_t bg_win_tile_data_start_p1;
  uint16_t bg_win_tile_data_start_p2;
  uint16_t win_tile_map_start;

  LogDebug("Window State: line_counter = %d, wx = %d, wy = %d", gb_state->saved.win_line_counter,
           gb_state->saved.regs.io.wx, gb_state->saved.regs.io.wy);

  // Window's X condition in a perfectly accurate pixel FIFO renderer would be checked every pixel, but since my
  // emulator currently uses a scanline based renderer, this should work just as well.
  gb_state->saved.wx_cond = (gb_state->saved.regs.io.wx < 167);

  if ((!gb_state->saved.wy_cond) || (!gb_state->saved.wx_cond) || (!(gb_state->saved.regs.io.lcdc & LCDC_WIN_ENABLE))) {
    gb_state->saved.win_line_blank = true;
    return;
  }
  gb_state->saved.win_line_blank = false;

  if (gb_state->saved.regs.io.lcdc & LCDC_BG_WIN_TILE_DATA_AREA) {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK0_START;
  } else {
    bg_win_tile_data_start_p1 = GB_TILEDATA_BLOCK2_START;
  }
  bg_win_tile_data_start_p2 = GB_TILEDATA_BLOCK1_START;

  if (gb_state->saved.regs.io.lcdc & LCDC_WIN_TILEMAP) {
    win_tile_map_start = GB_TILEMAP_BLOCK1_START;
  } else {
    win_tile_map_start = GB_TILEMAP_BLOCK0_START;
  }

  // TODO: Check if this is actually needed, I don't think it is.
  SDL_SetSurfacePalette(target, gb_state->video.sdl_bg_palette);
  for (int i = 0; i < 32; i++) {
    const int      x               = i;
    const int      y               = gb_state->saved.win_line_counter / 8;
    const uint8_t  tile_data_index = gb_read_mem(gb_state, win_tile_map_start + x + (y * 32));
    const uint16_t tile_data_addr  = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                     ((tile_data_index % 128) * 16);
    uint8_t        display_x       = (x * 8) + gb_state->saved.regs.io.wx - 7;
    uint8_t        display_y       = (gb_state->saved.regs.io.ly / 8) * 8;
    if (display_x < GB_DISPLAY_WIDTH && display_y < GB_DISPLAY_HEIGHT)
      gb_draw_tile_to_line(gb_state, target, gb_state->video.sdl_bg_palette, display_x, display_y, tile_data_addr,
                           SDL_FLIP_NONE);
  }
  gb_state->saved.win_line_counter++;
}
static void gb_render_objs(struct gb_state *gb_state, SDL_Surface *target, SDL_Surface *priority_target) {
  bool success;
  success = SDL_ClearSurface(target, 0, 0, 0, 0);
  GB_assert(success);
  success = SDL_ClearSurface(priority_target, 0, 0, 0, 0);
  GB_assert(success);
  if (!(gb_state->saved.regs.io.lcdc & LCDC_OBJ_ENABLE)) return;
  // TODO: I need to change this to choose the (up to) ten objects on this line in OAM_SCAN. I'll also want to properly
  // sort objects with equal X positions so that the first one has higher draw priority.
  for (int i = 0; i < 10; i++) {
    uint8_t                 tile_idx;
    const struct oam_entry *oam_entry = gb_state->saved.oam_entries[i];
    if (oam_entry == NULL) break;
    uint32_t flags = 0;
    if (oam_entry->x_flip) flags |= SDL_FLIP_HORIZONTAL;
    if (oam_entry->y_flip) flags |= SDL_FLIP_VERTICAL;
    SDL_Palette *palette;
    if (oam_entry->dmg_palette)
      palette = gb_state->video.sdl_obj_palette_1;
    else
      palette = gb_state->video.sdl_obj_palette_0;
    bool draw_double_height = (gb_state->saved.regs.io.lcdc & LCDC_OBJ_SIZE) >> 2;
    tile_idx                = oam_entry->index;
    if (draw_double_height) {
      tile_idx &= 0b1111'1110;
    }
    int x = oam_entry->x_pos - 8;
    int y = oam_entry->y_pos - 16;
    if (oam_entry->y_flip && draw_double_height) {
      y += 8;
    }
    if (x >= GB_DISPLAY_WIDTH) continue;
  draw_obj:
    if (oam_entry->priority) {
      gb_draw_tile_to_line(gb_state, priority_target, palette, x, y, 0x8000 + (tile_idx * 16), SDL_FlipMode(flags));
    } else {
      gb_draw_tile_to_line(gb_state, target, palette, x, y, 0x8000 + (tile_idx * 16), SDL_FlipMode(flags));
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
      .y = gb_state->saved.regs.io.ly,
      .w = GB_DISPLAY_WIDTH,
      .h = 1,
  };
  SDL_Surface *locked_texture;
  success = SDL_LockTextureToSurface(gb_state->video.sdl_composite_target_back, &line_rect, &locked_texture);
  GB_assert(success);

  if (gb_state->dbg.clear_composite) {
    SDL_ClearSurface(locked_texture, 0, 1, 0, 1);
  }

  // all intermediate targets should have equal dimensions to the locked line (w=GB_DISPLAY_WIDTH h=1)
  GB_assert(locked_texture->h == gb_state->video.sdl_bg_target->h);
  GB_assert(locked_texture->w == gb_state->video.sdl_bg_target->w);

  GB_assert(locked_texture->h == gb_state->video.sdl_obj_priority_target->h);
  GB_assert(locked_texture->w == gb_state->video.sdl_obj_priority_target->w);

  GB_assert(locked_texture->h == gb_state->video.sdl_obj_target->h);
  GB_assert(locked_texture->w == gb_state->video.sdl_obj_target->w);

  for (int i = 0; i < 160; i++) {
    // RULES: 
    //   1. Window always covers Background.
    //   2. Prio Obj's always get covered by BG/Win if the pixel is index 1-3.

    // UNKNOWN:
    //   1. If BG is index 1, and Win covers with index 0, and prio obj is drawn over, does the prio obj show, or not show?
  }

  // if (!gb_state->dbg.hide_bg) {
  //   // bg and win use the same palette
  //   SDL_SetSurfacePalette(gb_state->video.sdl_bg_target, gb_state->video.sdl_bg_palette);
  //   SDL_BlitSurface(gb_state->video.sdl_bg_target, NULL, locked_texture, NULL);
  // }

  // // TODO: Adjust width properly
  // SDL_Rect win_rect = {
  //     .x = gb_state->saved.regs.io.wx - 7,
  //     .y = 0,
  //     .w = GB_DISPLAY_WIDTH,
  //     .h = 1,
  // };
  // if (!gb_state->dbg.hide_win) {
  //   if (!gb_state->saved.win_line_blank) {
  //     SDL_SetSurfacePalette(gb_state->video.sdl_win_target, gb_state->video.sdl_bg_palette);
  //     SDL_BlitSurface(gb_state->video.sdl_win_target, &win_rect, locked_texture, &win_rect);
  //   }
  // }

  // if (!gb_state->dbg.hide_objs) {
  //   SDL_BlitSurface(gb_state->video.sdl_obj_priority_target, NULL, locked_texture, NULL);
  // }

  // if (!gb_state->dbg.hide_bg) {
  //   SDL_SetSurfacePalette(gb_state->video.sdl_bg_target, gb_state->video.sdl_bg_trans0_palette);
  //   SDL_BlitSurface(gb_state->video.sdl_bg_target, NULL, locked_texture, NULL);
  // }

  // if (!gb_state->dbg.hide_win) {
  //   if (!gb_state->saved.win_line_blank) {
  //     SDL_SetSurfacePalette(gb_state->video.sdl_win_target, gb_state->video.sdl_bg_trans0_palette);
  //     SDL_BlitSurface(gb_state->video.sdl_win_target, &win_rect, locked_texture, &win_rect);
  //   }
  // }

  // if (!gb_state->dbg.hide_objs) {
  //   SDL_BlitSurface(gb_state->video.sdl_obj_target, NULL, locked_texture, NULL);
  // }

  SDL_UnlockTexture(gb_state->video.sdl_composite_target_back);
}

void gb_read_oam_entries(struct gb_state *gb_state) {
  // Window's Y condition is checked for at the start of mode 2 (oam scan) every line.
  if (gb_state->saved.regs.io.ly == gb_state->saved.regs.io.wy) {
    gb_state->saved.wy_cond = true;
  }
  uint8_t oam_entries_pos = 0;
  for (int i = 0; i < 40; i++) {
    if (oam_entries_pos >= 10) {
      break;
    }
    const struct oam_entry *oam_entry          = get_oam_entry(gb_state, i);
    const bool              draw_double_height = (gb_state->saved.regs.io.lcdc & LCDC_OBJ_SIZE) >> 2;
    if (!gb_is_tile_in_scanline(gb_state, oam_entry->y_pos - 16, (draw_double_height) ? 16 : 8)) {
      continue;
    }
    // Sort by x_pos, if x_pos is equal to another entry's x_pos make sure first in oam_mem has highest priority (the
    // final entry in `gb_state->saved.oam_entries` is highest priority since it's the last to be drawn).
    bool inserted = false;
    for (int j = 0; j < oam_entries_pos; j++) {
      if (gb_state->saved.oam_entries[j]->x_pos <= oam_entry->x_pos) {
        memmove(&gb_state->saved.oam_entries[j + 1], &gb_state->saved.oam_entries[j],
                (oam_entries_pos - j) * sizeof(struct oam_entry *));
        gb_state->saved.oam_entries[j] = oam_entry;
        oam_entries_pos++;
        inserted = true;
        break;
      }
    }
    if (!inserted) gb_state->saved.oam_entries[oam_entries_pos++] = oam_entry;
  }
  if (oam_entries_pos < 10) {
    gb_state->saved.oam_entries[oam_entries_pos] = NULL;
  }
}

void gb_draw(struct gb_state *gb_state) {

  TracyCZoneN(update_palettes_ctx, "Palette Update", true);
  update_palettes(gb_state);
  TracyCZoneEnd(update_palettes_ctx);
  TracyCZoneN(render_bg_ctx, "Background Render", true);
  gb_render_bg(gb_state, gb_state->video.sdl_bg_target);
  TracyCZoneEnd(render_bg_ctx);
  TracyCZoneN(render_win_ctx, "Window Render", true);
  gb_render_win(gb_state, gb_state->video.sdl_win_target);
  TracyCZoneEnd(render_win_ctx);
  TracyCZoneN(render_objs_ctx, "Object Render", true);
  gb_render_objs(gb_state, gb_state->video.sdl_obj_target, gb_state->video.sdl_obj_priority_target);
  TracyCZoneEnd(render_objs_ctx);
}

void gb_display_clear(gb_state_t *gb_state) {
  SDL_Surface *locked_texture;
  // We just clear the entire front buffer in one call here. There's no need to flip textures since this all happens at
  // once (meaning there is no risk of displaying partial frames).
  CheckedSDL(LockTextureToSurface(gb_state->video.sdl_composite_target_front, NULL, &locked_texture));
  SDL_ClearSurface(locked_texture, 1.0, 1.0, 1.0, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_UnlockTexture(gb_state->video.sdl_composite_target_front);
}

void gb_display_render(gb_state_t *gb_state) {
  // We render the front buffer then the imgui UI to make sure we don't display partial frames.
  CheckedSDL(SetRenderTarget(gb_state->video.sdl_renderer, NULL));
  CheckedSDL(SetRenderDrawColorFloat(gb_state->video.sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT));
  CheckedSDL(RenderClear(gb_state->video.sdl_renderer));
  CheckedSDL(RenderTexture(gb_state->video.sdl_renderer, gb_state->video.sdl_composite_target_front, NULL, NULL));
}

// called on vblank
void gb_flip_frame(struct gb_state *gb_state) {

  uint64_t curr_ticks_ns                 = SDL_GetTicksNS();
  gb_state->dbg.ns_last_frametime        = curr_ticks_ns - gb_state->dbg.ns_elapsed_last_gb_vsync;
  gb_state->dbg.ns_elapsed_last_gb_vsync = curr_ticks_ns;

  gb_state->saved.win_line_counter = 0;
  gb_state->saved.wy_cond          = 0;

  SDL_Texture *tmp;
  tmp                                        = gb_state->video.sdl_composite_target_front;
  gb_state->video.sdl_composite_target_front = gb_state->video.sdl_composite_target_back;
  gb_state->video.sdl_composite_target_back  = tmp;

  TracyCFrameMark
}
