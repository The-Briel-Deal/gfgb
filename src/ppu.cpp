#define GB_LOG_CATEGORY GB_LOG_CATEGORY_PPU
#include "ppu.h"
#include "common.h"

#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <format>

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
    gb_state->headless_mode = true;
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

  gb_state->sdl_composite_target_front = SDL_CreateTexture(
      gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  GB_assert(gb_state->sdl_composite_target_front != NULL);
  gb_state->sdl_composite_target_back = SDL_CreateTexture(
      gb_state->sdl_renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT);
  GB_assert(gb_state->sdl_composite_target_back != NULL);

  // Initialize ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

  ImGui::StyleColorsDark();

  float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  // Setup scaling
  ImGuiStyle &style = ImGui::GetStyle();
  style.ScaleAllSizes(main_scale); // Bake a fixed style scale. (until we have a solution for dynamic style scaling,
                                   // changing this requires resetting Style + calling this again)
  style.FontScaleDpi = main_scale; // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true
                                   // automatically overrides this for every window depending on the current monitor)

  // Setup Platform/Renderer backends
  ImGui_ImplSDL3_InitForSDLRenderer(gb_state->sdl_window, gb_state->sdl_renderer);
  ImGui_ImplSDLRenderer3_Init(gb_state->sdl_renderer);

  gb_state->enable_fs_dockspace = true;
  gb_state->video_initialized   = true;

  return true;
}

void gb_video_free(struct gb_state *gb_state) {
  if (!gb_state->video_initialized) return;
  // free all textures
  for (int i = 0; i < DMG_N_TILEDATA_ADDRESSES; i++) {
    if (gb_state->textures[i] != NULL) {
      SDL_DestroyTexture(gb_state->textures[i]);
      gb_state->textures[i] = NULL;
    }
  }

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyPalette(gb_state->sdl_bg_palette);
  gb_state->sdl_bg_palette = NULL;
  SDL_DestroyPalette(gb_state->sdl_bg_trans0_palette);
  gb_state->sdl_bg_trans0_palette = NULL;
  SDL_DestroyPalette(gb_state->sdl_obj_palette_0);
  gb_state->sdl_obj_palette_0 = NULL;
  SDL_DestroyPalette(gb_state->sdl_obj_palette_1);
  gb_state->sdl_obj_palette_1 = NULL;
  SDL_DestroySurface(gb_state->sdl_bg_target);
  gb_state->sdl_bg_target = NULL;
  SDL_DestroySurface(gb_state->sdl_win_target);
  gb_state->sdl_win_target = NULL;
  SDL_DestroySurface(gb_state->sdl_obj_target);
  gb_state->sdl_obj_target = NULL;
  SDL_DestroySurface(gb_state->sdl_obj_priority_target);
  gb_state->sdl_obj_priority_target = NULL;
  SDL_DestroyTexture(gb_state->sdl_composite_target_front);
  gb_state->sdl_composite_target_front = NULL;
  SDL_DestroyTexture(gb_state->sdl_composite_target_back);
  gb_state->sdl_composite_target_back = NULL;
  SDL_DestroyRenderer(gb_state->sdl_renderer);
  gb_state->sdl_renderer = NULL;
  SDL_DestroyWindow(gb_state->sdl_window);
  gb_state->sdl_window = NULL;
}

bool gb_video_handle_sdl_event(struct gb_state *gb_state, SDL_Event *event) {
  (void)gb_state;
  auto io = ImGui::GetIO();
  ImGui_ImplSDL3_ProcessEvent(event);
  // If ImGui wants keyboard events don't try to handle keyboard input myself. I don't do anything with mouse atm, this
  // may change down the road.
  return io.WantCaptureKeyboard;
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

  uint8_t *gb_tile = (uint8_t *)gb_unmap_address(gb_state, tile_addr);
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
    const uint8_t  tile_data_index = gb_read_mem8(gb_state, bg_tile_map_start + i);
    const uint16_t tile_data_addr  = (tile_data_index < 128 ? bg_win_tile_data_start_p1 : bg_win_tile_data_start_p2) +
                                    ((tile_data_index % 128) * 16);
    // Use signed integers when calculating the display position since we still display tiles where y = 0 to -7
    int16_t display_x = (x * 8) - gb_state->regs.io.scx;
    int16_t display_y = (y * 8) - gb_state->regs.io.scy;
    if (display_x < -7) display_x += 256;
    if (display_y < -7) display_y += 256;
    GB_assert(display_x > -8 && display_x < 256);
    GB_assert(display_y > -8 && display_y < 256);
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
    const uint8_t  tile_data_index = gb_read_mem8(gb_state, win_tile_map_start + x + (y * 32));
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
    if (x >= GB_DISPLAY_WIDTH) continue;
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
  success = SDL_LockTextureToSurface(gb_state->sdl_composite_target_back, &line_rect, &locked_texture);
  GB_assert(success);

  if (gb_state->dbg_clear_composite) {
    SDL_ClearSurface(locked_texture, 0, 1, 0, 1);
  }

  // all intermediate targets should have equal dimensions to the locked line (w=GB_DISPLAY_WIDTH h=1)
  GB_assert(locked_texture->h == gb_state->sdl_bg_target->h);
  GB_assert(locked_texture->w == gb_state->sdl_bg_target->w);

  GB_assert(locked_texture->h == gb_state->sdl_obj_priority_target->h);
  GB_assert(locked_texture->w == gb_state->sdl_obj_priority_target->w);

  GB_assert(locked_texture->h == gb_state->sdl_obj_target->h);
  GB_assert(locked_texture->w == gb_state->sdl_obj_target->w);

  if (!gb_state->dbg_hide_bg) {
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
  if (!gb_state->dbg_hide_win) {
    if (!gb_state->win_line_blank) {
      SDL_SetSurfacePalette(gb_state->sdl_win_target, gb_state->sdl_bg_palette);
      SDL_BlitSurface(gb_state->sdl_win_target, &win_rect, locked_texture, &win_rect);
    }
  }

  if (!gb_state->dbg_hide_objs) {
    SDL_BlitSurface(gb_state->sdl_obj_priority_target, NULL, locked_texture, NULL);
  }

  if (!gb_state->dbg_hide_bg) {
    SDL_SetSurfacePalette(gb_state->sdl_bg_target, gb_state->sdl_bg_trans0_palette);
    SDL_BlitSurface(gb_state->sdl_bg_target, NULL, locked_texture, NULL);
  }

  if (!gb_state->dbg_hide_win) {
    if (!gb_state->win_line_blank) {
      SDL_SetSurfacePalette(gb_state->sdl_win_target, gb_state->sdl_bg_trans0_palette);
      SDL_BlitSurface(gb_state->sdl_win_target, &win_rect, locked_texture, &win_rect);
    }
  }

  if (!gb_state->dbg_hide_objs) {
    SDL_BlitSurface(gb_state->sdl_obj_target, NULL, locked_texture, NULL);
  }

  SDL_UnlockTexture(gb_state->sdl_composite_target_back);
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

static void gb_imgui_show_mem_val(struct gb_state *gb_state, const char *name, const uint16_t addr) {
  std::string formatted_text = std::format("{0:s} ({1:#04x}) Value is:\n"
                                           "  Hex: {2:#04x}\n"
                                           "  Dec: {2:d}\n"
                                           "  Bin: {2:#010b}",
                                           name, addr, gb_read_mem8(gb_state, addr));
  ImGui::TextUnformatted(formatted_text.c_str());
}
static void gb_imgui_show_val(const char *name, const uint8_t val) {
  std::string formatted_text = std::format("{0:s}:\n"
                                           "  Hex: {1:#04x}\n"
                                           "  Dec: {1:d}\n"
                                           "  Bin: {1:#010b}",
                                           name, val);
  ImGui::TextUnformatted(formatted_text.c_str());
}

static void gb_imgui_show_val(const char *name, const uint16_t val) {
  std::string formatted_text = std::format("{0:s}:\n"
                                           "  Hex: {1:#06x}\n"
                                           "  Dec: {1:d}\n"
                                           "  Bin: {1:#018b}",
                                           name, val);
  ImGui::TextUnformatted(formatted_text.c_str());
}

void gb_display_clear(gb_state_t *gb_state) {
  SDL_Surface *locked_texture;
  // We just clear the entire front buffer in one call here. There's no need to flip textures since this all happens at
  // once (meaning there is no risk of displaying partial frames).
  GB_CheckSDLCall(SDL_LockTextureToSurface(gb_state->sdl_composite_target_front, NULL, &locked_texture));
  SDL_ClearSurface(locked_texture, 1.0, 1.0, 1.0, SDL_ALPHA_OPAQUE_FLOAT);
  SDL_UnlockTexture(gb_state->sdl_composite_target_front);
}

void gb_display_render(gb_state_t *gb_state) {
  // We render the front buffer then the imgui UI to make sure we don't display partial frames.
  GB_CheckSDLCall(SDL_SetRenderTarget(gb_state->sdl_renderer, NULL));
  GB_CheckSDLCall(SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0, SDL_ALPHA_OPAQUE_FLOAT));
  GB_CheckSDLCall(SDL_RenderClear(gb_state->sdl_renderer));
  GB_CheckSDLCall(SDL_RenderTexture(gb_state->sdl_renderer, gb_state->sdl_composite_target_front, NULL, NULL));
}

void gb_imgui_render(struct gb_state *gb_state) {
  gb_imgui_state_t *imgui_state = &gb_state->imgui_state;
  ImGuiIO          &io          = ImGui::GetIO();
  (void)io;

  GB_CheckSDLCall(SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED));
  // Start ImGui frame
  ImGui_ImplSDLRenderer3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();
  if (gb_state->enable_fs_dockspace) {
    ImGui::DockSpaceOverViewport();
  }

  {
    ImGui::Begin("Display Viewport");
    // always keep the correct aspect ratio based off of the window width.
    ImVec2 win_size;
    win_size.x = ImGui::GetWindowWidth();
    win_size.y = ((win_size.x * GB_DISPLAY_HEIGHT) / GB_DISPLAY_WIDTH);
    ImGui::Image((ImTextureID)(intptr_t)gb_state->sdl_composite_target_front, win_size);
    ImGui::End();
  }
  {
    ImGui::Begin("GB State");

    if (ImGui::TreeNodeEx("Execution", ImGuiTreeNodeFlags_Framed)) {
      // Framerate
      // TODO: I might want to track average framerate as well as 1% lows to identify stuttering if that becomes an
      // issue.
      if (ImGui::Button("Reset")) {
        gb_state_reset(gb_state);
      }
      {
        ImGui::BeginDisabled(!gb_state->execution_paused);
        if (ImGui::Button("Step Instruction")) {
          gb_state->dbg_step_inst_count++;
        }
        ImGui::EndDisabled();
      }
      float last_frametime = (float)gb_state->ns_last_frametime / NS_PER_SEC;
      float last_frame_fps = 0.0f;
      if (last_frametime != 0.0f) {
        last_frame_fps = 1 / last_frametime;
      }
      ImGui::Value("Last Frametime", last_frametime, "%.6f");
      ImGui::Value("Last Frame FPS", last_frame_fps, "%.6f");

      ImGui::Checkbox("Paused", &gb_state->execution_paused);
      ImGui::Checkbox("Halted", &gb_state->halted);
      ImGui::TextUnformatted("Addr:");
      ImGui::SameLine();
      ImGui::InputScalar("##addr", ImGuiDataType_U16, &imgui_state->breakpoint_addr, NULL, NULL, "%.4x");
      if (ImGui::Button("Set Breakpoint")) {
        gb_state->breakpoints->push_back({.addr = imgui_state->breakpoint_addr});
      }
      int i = 1;
      for (gb_breakpoint_t bp : *gb_state->breakpoints) {
        ImGui::Text("Breakpoint %d: %.4x", i++, bp.addr);
      }
      ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Serial Port Output", ImGuiTreeNodeFlags_Framed)) {
      ImGui::TextUnformatted(gb_state->serial_port_output_string->c_str());
      ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Settings", ImGuiTreeNodeFlags_Framed)) {
      ImGui::SliderFloat("Internal GB Speed", &gb_state->dbg_speed_factor, 0.0f, 10.0f);
      ImGui::Checkbox("Enable Fullscreen Dockspace", &gb_state->enable_fs_dockspace);
      ImGui::Checkbox("Pause on Error", &gb_state->pause_on_err);
      ImGui::Checkbox("Print Instructions", &gb_state->dbg_print_inst_during_exec);
      ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Layers", ImGuiTreeNodeFlags_Framed)) {
      ImGui::Checkbox("Clear Before Render", &gb_state->dbg_clear_composite);
      ImGui::Checkbox("Background Hidden", &gb_state->dbg_hide_bg);
      ImGui::Checkbox("Window Hidden", &gb_state->dbg_hide_win);
      ImGui::Checkbox("Objs Hidden", &gb_state->dbg_hide_objs);
      ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Inspect Memory", ImGuiTreeNodeFlags_Framed)) {

      ImGui::TextUnformatted("Addr:");
      ImGui::SameLine();
      ImGui::InputScalar("##addr", ImGuiDataType_U16, &imgui_state->mem_inspect_addr, NULL, NULL, "%.4x");

      ImGui::TextUnformatted("Val: ");
      ImGui::SameLine();
      ImGui::InputScalar("##val", ImGuiDataType_U8, &imgui_state->mem_inspect_val, NULL, NULL, "%.2x");

      if (ImGui::Button("Read")) {
        imgui_state->mem_inspect_last_read_addr = imgui_state->mem_inspect_addr;
        imgui_state->mem_inspect_last_read_val  = gb_read_mem8(gb_state, imgui_state->mem_inspect_addr);
      }
      ImGui::SameLine();
      if (ImGui::Button("Write")) {
        imgui_state->mem_inspect_last_write_addr = imgui_state->mem_inspect_addr;
        imgui_state->mem_inspect_last_write_val  = imgui_state->mem_inspect_val;
        gb_write_mem8(gb_state, imgui_state->mem_inspect_last_write_addr, imgui_state->mem_inspect_last_write_val);
      }
      std::string formatted_read_text =
          std::format("Value read from addr {0:#06x} is:\n"
                      "  Hex: {1:#04x}\n"
                      "  Dec: {1:d}\n"
                      "  Bin: {1:#010b}",
                      imgui_state->mem_inspect_last_read_addr, imgui_state->mem_inspect_last_read_val);
      std::string formatted_write_text =
          std::format("Value written to addr {0:#06x} is:\n"
                      "  Hex: {1:#04x}\n"
                      "  Dec: {1:d}\n"
                      "  Bin: {1:#010b}",
                      imgui_state->mem_inspect_last_write_addr, imgui_state->mem_inspect_last_write_val);
      ImGui::TextUnformatted(formatted_read_text.c_str());
      ImGui::TextUnformatted(formatted_write_text.c_str());
      ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Joy-Pad State", ImGuiTreeNodeFlags_Framed)) {
      ImGui::Value("Up", gb_state->joy_pad_state.dpad_up);
      ImGui::Value("Down", gb_state->joy_pad_state.dpad_down);
      ImGui::Value("Left", gb_state->joy_pad_state.dpad_left);
      ImGui::Value("Right", gb_state->joy_pad_state.dpad_right);
      ImGui::Value("A Button", gb_state->joy_pad_state.button_a);
      ImGui::Value("B Button", gb_state->joy_pad_state.button_b);
      ImGui::Value("Start Button", gb_state->joy_pad_state.button_start);
      ImGui::Value("Select Button", gb_state->joy_pad_state.button_select);
      ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Reg Values", ImGuiTreeNodeFlags_Framed)) {
      gb_imgui_show_val("A", gb_state->regs.a);
      gb_imgui_show_val("B", gb_state->regs.b);
      gb_imgui_show_val("C", gb_state->regs.c);
      gb_imgui_show_val("D", gb_state->regs.d);
      gb_imgui_show_val("E", gb_state->regs.e);
      gb_imgui_show_val("F", gb_state->regs.f);
      gb_imgui_show_val("H", gb_state->regs.h);
      gb_imgui_show_val("L", gb_state->regs.l);
      gb_imgui_show_val("PC", gb_state->regs.pc);
      gb_imgui_show_val("SP", gb_state->regs.sp);
      ImGui::Value("IME", gb_state->regs.io.ime);
      ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("IO Reg Values", ImGuiTreeNodeFlags_Framed)) {
      for (io_reg_addr_t io_reg : io_regs) {
        gb_imgui_show_mem_val(gb_state, gb_io_reg_name(io_reg), io_reg);
      }
      ImGui::TreePop();
    }

    ImGui::End();
    ImGui::Render();

    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), gb_state->sdl_renderer);

    GB_CheckSDLCall(SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, GB_DISPLAY_WIDTH, GB_DISPLAY_HEIGHT,
                                                     SDL_LOGICAL_PRESENTATION_LETTERBOX));
  }
}

// called on vblank
void gb_flip_frame(struct gb_state *gb_state) {

  uint64_t curr_ticks_ns             = SDL_GetTicksNS();
  gb_state->ns_last_frametime        = curr_ticks_ns - gb_state->ns_elapsed_last_gb_vsync;
  gb_state->ns_elapsed_last_gb_vsync = curr_ticks_ns;

  gb_state->win_line_counter         = 0;
  gb_state->wy_cond                  = 0;

  SDL_Texture *tmp;
  tmp                                  = gb_state->sdl_composite_target_front;
  gb_state->sdl_composite_target_front = gb_state->sdl_composite_target_back;
  gb_state->sdl_composite_target_back  = tmp;

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
      LogInfo("byte i=%d of indexed_8bit result (%.2x) is not equal to result (%.2x)\n", i, indexed_8bit_tile_result[i],
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
