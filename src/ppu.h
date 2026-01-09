#ifndef GB_PPU_H
#define GB_PPU_H

#include <stdint.h>

//! \brief Convert a gameboy tile to indexed msb2
//! The primary objective here is to make it so that I can send gameboy tiles to
//! SDL in a format that SDL understands.
//!
//! For example, `0b1010'1100 0b1100'1011` would turn into `0b1110'0100
//! 0b1101'1010`
//!
void gb_tile_to_msb2(uint8_t *gb_tile_in, uint8_t *msb2_tile_out);

#endif // GB_PPU_H
