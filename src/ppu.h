#ifndef GB_PPU_H
#define GB_PPU_H

#include <SDL3/SDL.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;

bool gb_video_init(struct gb_state *gb_state);
void gb_video_free(struct gb_state *gb_state);
void gb_video_handle_sdl_event(struct gb_state *gb_state, SDL_Event *event);

#define OBP0 0
#define OBP1 1

struct __attribute__((packed)) oam_entry {
  uint8_t      y_pos : 8;
  uint8_t      x_pos : 8;
  uint8_t      index : 8;
  unsigned int cgb_palette : 3;
  bool         bank : 1;
  bool         dmg_palette : 1;
  bool         x_flip : 1;
  bool         y_flip : 1;
  bool         priority : 1;
};

void gb_read_oam_entries(struct gb_state *gb_state); // on OAM-Scan
void gb_draw(struct gb_state *gb_state);             // on Drawing-Pixels
void gb_composite_line(struct gb_state *gb_state);   // on H-Blank
void gb_present(struct gb_state *gb_state);          // on V-Blank
                                                     //

#ifdef __cplusplus
}
#endif

#endif // GB_PPU_H
