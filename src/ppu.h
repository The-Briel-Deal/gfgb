#ifndef GB_PPU_H
#define GB_PPU_H

#include "common.h"
#include <stdint.h>

//! \brief Convert a gameboy tile to indexed msb2
//!
//! The primary objective here is to make it so that I can send gameboy tiles to
//! SDL in a format that SDL understands.
//!
//! For example, `0b1010'1100 0b1100'1011` would turn into
//! `0b1110'0100 0b1101'1010`
//!
void gb_tile_to_8bit_indexed(uint8_t *gb_tile_in, uint8_t *msb2_tile_out);

SDL_Texture *get_texture_for_tile(struct gb_state *gb_state, uint16_t tile_addr);

#define OBP0 0
#define OBP1 1

struct __attribute__((packed)) oam_entry {
  uint8_t y_pos : 8;
  uint8_t x_pos : 8;
  uint8_t index : 8;
  bool priority : 1;
  bool y_flip : 1;
  bool x_flip : 1;
  bool dmg_palette : 1;
  bool bank : 1;
  unsigned int cgb_palette : 3;
};

struct oam_entry get_oam_entry(struct gb_state *gb_state, uint8_t index);

#endif // GB_PPU_H
