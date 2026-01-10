#ifndef GB_PPU_H
#define GB_PPU_H

#include "common.h"

//! \brief Convert a gameboy tile to indexed msb2
//!
//! The primary objective here is to make it so that I can send gameboy tiles to
//! SDL in a format that SDL understands.
//!
//! For example, `0b1010'1100 0b1100'1011` would turn into
//! `0b1110'0100 0b1101'1010`
//!
void gb_tile_to_8bit_indexed(uint8_t *gb_tile_in, uint8_t *msb2_tile_out);

SDL_Texture *get_texture_for_tile(struct gb_state *gb_state,
                                  uint16_t tile_addr);

#endif // GB_PPU_H
