#ifndef GB_PPU_H
#define GB_PPU_H

#include <SDL3/SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;
typedef struct gb_state gb_state_t;

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

typedef enum gb_ppu_mode {
  HBLANK         = 0,
  VBLANK         = 1,
  OAM_SCAN       = 2,
  DRAWING_PIXELS = 3,
} gb_ppu_mode_t;

bool gb_video_init(gb_state_t *gb_state);
void gb_video_free(gb_state_t *gb_state);

#define OBP0 0
#define OBP1 1

typedef struct __attribute__((packed)) oam_entry {
  uint8_t      y_pos : 8;
  uint8_t      x_pos : 8;
  uint8_t      index : 8;
  unsigned int cgb_palette : 3; // CGB Only (as you can assume from the name lol)
  bool         bank : 1;        // CGB Only
  bool         dmg_palette : 1;
  bool         x_flip : 1;
  bool         y_flip : 1;
  bool         priority : 1;
} oam_entry_t;

void gb_draw_tile_to_surface(struct gb_state *gb_state, SDL_Surface *target, SDL_Palette *palette, int x, int y,
                             uint16_t tile_addr, SDL_FlipMode flip_mode);

void gb_tile_to_8bit_indexed(uint8_t *tile_in, uint8_t *tile_out);
void gb_read_oam_entries(gb_state_t *gb_state); // on OAM-Scan
void gb_draw(gb_state_t *gb_state);             // on Drawing-Pixels
void gb_composite_line(gb_state_t *gb_state);   // on H-Blank
void gb_flip_frame(gb_state_t *gb_state);       // on V-Blank

void gb_display_clear(gb_state_t *gb_state);
void gb_display_render(gb_state_t *gb_state);

#ifdef __cplusplus
}
#endif

#endif // GB_PPU_H
