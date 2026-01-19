#ifndef GB_COMMON_H
#define GB_COMMON_H

#include "cpu.h"
#include "disassemble.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_log.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NOT_IMPLEMENTED(msg)                                                                                           \
  {                                                                                                                    \
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION, "This functionality is not yet implemented: %s", msg);               \
    abort();                                                                                                           \
  }

#define KB(n)                      (1024 * n)

#define NIBBLE0(byte)              ((byte & 0xF0) >> 4)
#define NIBBLE1(byte)              ((byte & 0x0F) >> 0)

#define OCTAL0(byte)               ((byte & 0700) >> 6)
#define OCTAL1(byte)               ((byte & 0070) >> 3)
#define OCTAL2(byte)               ((byte & 0007) >> 0)

#define CRUMB0(byte)               ((byte & 0b11000000) >> 6)
#define CRUMB1(byte)               ((byte & 0b00110000) >> 4)
#define CRUMB2(byte)               ((byte & 0b00001100) >> 2)
#define CRUMB3(byte)               ((byte & 0b00000011) >> 0)

#define GB_BG_WIDTH                256
#define GB_BG_HEIGHT               256

#define GB_DISPLAY_WIDTH           160
#define GB_DISPLAY_HEIGHT          144

#define GB_TILEDATA_BLOCK0_START   0x8000
#define GB_TILEDATA_BLOCK1_START   0x8800
#define GB_TILEDATA_BLOCK2_START   0x9000

#define GB_TILEMAP_BLOCK0_START    0x9800
#define GB_TILEMAP_BLOCK1_START    0x9C00

#define DMG_PALETTE_N_COLORS       4
#define DMG_N_TILEDATA_ADDRESSES   (128 * 3)
#define DMG_BOOTROM_SIZE           0x100

#define NS_PER_SEC                 (1 * 1000 * 1000 * 1000)

#define DMG_CLOCK_HZ               (1 << 22)

#define DOTS_PER_FRAME             70224

#define ROM0_START                 0x0000
#define ROM0_END                   0x3FFF

#define ROMN_START                 0x4000
#define ROMN_END                   0x7FFF

// VRAM on CGB is switchable across 2 8KB banks, on DMG this is just one 8KB
// block. I won't worry about this until DMG is finished.
#define VRAM_START                 0x8000
#define VRAM_END                   0x9FFF

#define ERAM_START                 0xA000
#define ERAM_END                   0xBFFF

// This is split in two on the CGB and the second half is switchable. I'm just
// worrying about DMG for now.
#define WRAM_START                 0xC000
#define WRAM_END                   0xDFFF

#define IO_REG_START               0xFF00
#define IO_REG_END                 0xFF7F

#define HRAM_START                 0xFF80
#define HRAM_END                   0xFFFE

// This is little endian, so the number is constructed as r2,r1
#define COMBINED_REG(regs, r1, r2) (((uint16_t)regs.r1 << 8) | ((uint16_t)regs.r2 << 0))
#define SET_COMBINED_REG(regs, r1, r2, val)                                                                            \
  {                                                                                                                    \
    regs.r1 = (0xFF00 & val) >> 8;                                                                                     \
    regs.r2 = (0x00FF & val) >> 0;                                                                                     \
  }

struct gb_state {
  SDL_Window *sdl_window;
  SDL_Renderer *sdl_renderer;
  SDL_Palette *sdl_palette;
  struct regs {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t f;
    uint8_t h;
    uint8_t l;
    uint16_t sp;
    uint16_t pc;
    struct io_regs {
      uint8_t sc; // serial control

      uint8_t tima; // timer counter
      uint8_t tma;  // timer modulo
      uint8_t tac;  // timer control
      // Sound
      uint8_t nr10;
      uint8_t nr11;
      uint8_t nr12;
      uint8_t nr13;
      uint8_t nr14;

      uint8_t nr21;
      uint8_t nr22;
      uint8_t nr23;
      uint8_t nr24;

      uint8_t nr30;
      uint8_t nr31;
      uint8_t nr32;
      uint8_t nr33;
      uint8_t nr34;

      uint8_t nr41;
      uint8_t nr42;
      uint8_t nr43;
      uint8_t nr44;

      uint8_t nr50;
      uint8_t nr51;
      uint8_t nr52; // sound on/off

      uint8_t lcdc;
      uint8_t scy;
      uint8_t scx;
      uint8_t bg_pallete;
      uint8_t ie;  // interupt enable
      uint8_t if_; // interupt flag
      bool ime;    // interupt master enable
    } io;
  } regs;
  bool bootrom_mapped;
  bool bootrom_has_syms;
  bool rom_loaded;
  uint8_t bootrom[DMG_BOOTROM_SIZE];
  uint8_t rom0[KB(16)];
  uint8_t rom1[KB(16)];
  uint8_t wram[KB(8)];
  uint8_t vram[KB(8)];
  uint8_t eram[KB(8)];
  uint8_t hram[0x80];
  struct debug_symbol_list syms;
  SDL_Texture *textures[DMG_N_TILEDATA_ADDRESSES];

  FILE *serial_port_output;

  // Used for getting fps.
  uint64_t last_frame_ticks_ns;
};

enum io_reg_addr {
  IO_SB = 0xFF01,
  IO_SC = 0xFF02,
  IO_TIMA = 0xFF05,
  IO_TMA = 0xFF06,
  IO_TAC = 0xFF07,
  IO_NR10 = 0xFF10,
  IO_NR11 = 0xFF11,
  IO_NR12 = 0xFF12,
  IO_NR13 = 0xFF13,
  IO_NR14 = 0xFF14,
  IO_NR21 = 0xFF16,
  IO_NR22 = 0xFF17,
  IO_NR23 = 0xFF18,
  IO_NR24 = 0xFF19,
  IO_NR30 = 0xFF1A,
  IO_NR31 = 0xFF1B,
  IO_NR32 = 0xFF1C,
  IO_NR33 = 0xFF1D,
  IO_NR34 = 0xFF1E,
  IO_NR41 = 0xFF20,
  IO_NR42 = 0xFF21,
  IO_NR43 = 0xFF22,
  IO_NR44 = 0xFF23,
  IO_NR50 = 0xFF24,
  IO_NR51 = 0xFF25,
  IO_IF = 0xFF0F,
  IO_IE = 0xFFFF,
  IO_SND_ON = 0xFF26,
  IO_LCDC = 0xFF40,
  IO_SCY = 0xFF42,
  IO_SCX = 0xFF43,
  IO_LY = 0xFF44,
  IO_BGP = 0xFF47,
};

static inline uint32_t gb_dots() {
  uint64_t ticks_ns = SDL_GetTicksNS();
  ticks_ns %= NS_PER_SEC;
  uint32_t dots = ticks_ns / (NS_PER_SEC / DMG_CLOCK_HZ);
  return dots;
}

void *unmap_address(struct gb_state *gb_state, uint16_t addr);

uint8_t read_mem8(struct gb_state *gb_state, uint16_t addr);
void write_mem8(struct gb_state *gb_state, uint16_t addr, uint8_t val);

uint16_t read_mem16(struct gb_state *gb_state, uint16_t addr);
void write_mem16(struct gb_state *gb_state, uint16_t addr, uint16_t val);

void gb_state_init(struct gb_state *gb_state);
struct gb_state *gb_state_alloc();
void gb_state_free(struct gb_state *gb_state);

#endif // GB_COMMON_H
