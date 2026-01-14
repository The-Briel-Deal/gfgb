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

#define NOT_IMPLEMENTED(msg)                                                   \
  {                                                                            \
    SDL_LogCritical(SDL_LOG_CATEGORY_APPLICATION,                              \
                    "This functionality is not yet implemented: %s", msg);     \
    abort();                                                                   \
  }

#define KB(n)                    (1024 * n)

#define NIBBLE0(byte)            ((byte & 0xF0) >> 4)
#define NIBBLE1(byte)            ((byte & 0x0F) >> 0)

#define OCTAL0(byte)             ((byte & 0700) >> 6)
#define OCTAL1(byte)             ((byte & 0070) >> 3)
#define OCTAL2(byte)             ((byte & 0007) >> 0)

#define CRUMB0(byte)             ((byte & 0b11000000) >> 6)
#define CRUMB1(byte)             ((byte & 0b00110000) >> 4)
#define CRUMB2(byte)             ((byte & 0b00001100) >> 2)
#define CRUMB3(byte)             ((byte & 0b00000011) >> 0)

#define GB_BG_WIDTH              256
#define GB_BG_HEIGHT             256

#define GB_DISPLAY_WIDTH         160
#define GB_DISPLAY_HEIGHT        144

#define GB_TILEDATA_BLOCK0_START 0x8000
#define GB_TILEDATA_BLOCK1_START 0x8800
#define GB_TILEDATA_BLOCK2_START 0x9000

#define GB_TILEMAP_BLOCK0_START  0x9800
#define GB_TILEMAP_BLOCK1_START  0x9C00

#define DMG_PALETTE_N_COLORS     4
#define DMG_N_TILEDATA_ADDRESSES (128 * 3)
#define DMG_BOOTROM_SIZE         0x100

#define NS_PER_SEC               (1 * 1000 * 1000 * 1000)

#define DMG_CLOCK_HZ             (1 << 22)

#define DOTS_PER_FRAME           70224

// This is little endian, so the number is constructed as r2,r1
#define COMBINED_REG(regs, r1, r2)                                             \
  (((uint16_t)regs.r2 << 8) | ((uint16_t)regs.r1 << 0))
#define SET_COMBINED_REG(regs, r1, r2, val)                                    \
  {                                                                            \
    regs.r1 = (0x00FF & val) >> 0;                                             \
    regs.r2 = (0xFF00 & val) >> 8;                                             \
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

      uint8_t lcd_control;
      uint8_t bg_pallete;
      uint8_t ie;  // interupt enable
      uint8_t if_; // interupt flag
    } io;
  } regs;
  bool bootrom_mapped;
  bool bootrom_has_syms;
  bool rom_loaded;
  uint8_t bootrom[DMG_BOOTROM_SIZE];
  uint8_t rom0[KB(16)];
  uint8_t wram[KB(8)];
  uint8_t vram[KB(8)];
  uint8_t hram[0x80];
  struct debug_symbol_list syms;
  SDL_Texture *textures[DMG_N_TILEDATA_ADDRESSES];

  FILE *serial_port_output;

  // Used for getting fps.
  uint64_t last_frame_ticks_ns;
};

#define ROM0_START   0x0000
#define ROM0_END     0x3FFF

#define ROMN_START   0x4000
#define ROMN_END     0x7FFF

// VRAM on CGB is switchable across 2 8KB banks, on DMG this is just one 8KB
// block. I won't worry about this until DMG is finished.
#define VRAM_START   0x8000
#define VRAM_END     0x9FFF

#define ERAM_START   0xA000
#define ERAM_END     0xBFFF

// This is split in two on the CGB and the second half is switchable. I'm just
// worrying about DMG for now.
#define WRAM_START   0xC000
#define WRAM_END     0xDFFF

#define IO_REG_START 0xFF00
#define IO_REG_END   0xFF7F

#define HRAM_START   0xFF80
#define HRAM_END     0xFFFE

static void *unmap_address(struct gb_state *gb_state, uint16_t addr) {
  if (gb_state->bootrom_mapped && (addr < 0x0100)) {
    return &gb_state->bootrom[addr];
  }
  if (addr <= ROM0_END) {
    return &gb_state->rom0[addr - ROM0_START];
  } else if (addr <= ROMN_END) {
    NOT_IMPLEMENTED("Rom Bank 01-NN not implemented");
  } else if (addr <= VRAM_END) {
    return &gb_state->vram[addr - VRAM_START];
  } else if (addr <= ERAM_END) {
    NOT_IMPLEMENTED("External RAM not implemented");
  } else if (addr <= WRAM_END) {
    return &gb_state->wram[addr - WRAM_START];
  } else if (addr <= IO_REG_END) {
    NOT_IMPLEMENTED("Everything between WRAM and IO Regs not implemented");
  } else if (addr <= HRAM_END) {
    return &gb_state->hram[addr - HRAM_START];
  } else {
    NOT_IMPLEMENTED("Everything after Work RAM is not yet implemented");
  }
}

static inline uint32_t gb_dots() {
  uint64_t ticks_ns = SDL_GetTicksNS();
  ticks_ns %= NS_PER_SEC;
  uint32_t dots = ticks_ns / (NS_PER_SEC / DMG_CLOCK_HZ);
  return dots;
}

#define IO_SERIAL_TRANSFER 0xFF01
#define IO_SERIAL_CONTROL  0xFF02
#define IO_SND_ON          0xFF26
#define IO_LCDC            0xFF40
#define IO_LY              0xFF44
#define IO_BGP             0xFF47

static inline uint8_t read_mem8(struct gb_state *gb_state, uint16_t addr) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
               "Reading 8 bits from address 0x%.4X", addr);
  if (addr >= IO_REG_START && addr <= IO_REG_END) {
    switch (addr) {
    case IO_LY: {
      uint32_t dots = gb_dots();
      dots %= DOTS_PER_FRAME;
      uint8_t ly = dots / 456;
      assert(ly < 154);
      return ly;
    }
    case IO_LCDC: {
      return gb_state->regs.io.lcd_control;
    }
    default: NOT_IMPLEMENTED("IO Reg Not Implemented");
    }
  }

  uint8_t val = *((uint8_t *)unmap_address(gb_state, addr));
  return val;
}

static inline uint16_t read_mem16(struct gb_state *gb_state, uint16_t addr) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
               "Reading 16 bits from address 0x%.4X", addr);
  uint8_t *val_ptr = unmap_address(gb_state, addr);
  uint16_t val = 0x0000;
  val |= val_ptr[0] << 0;
  val |= val_ptr[1] << 8;
  return val;
}

static inline void write_mem8(struct gb_state *gb_state, uint16_t addr,
                              uint8_t val) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
               "Writing val 0x%.2X to address 0x%.4X", val, addr);
  if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
    switch (addr) {
    case 0xFF10: gb_state->regs.io.nr10 = val; return;
    case 0xFF11: gb_state->regs.io.nr11 = val; return;
    case 0xFF12: gb_state->regs.io.nr12 = val; return;
    case 0xFF13: gb_state->regs.io.nr13 = val; return;
    case 0xFF14: gb_state->regs.io.nr14 = val; return;
    case 0xFF16: gb_state->regs.io.nr21 = val; return;
    case 0xFF17: gb_state->regs.io.nr22 = val; return;
    case 0xFF18: gb_state->regs.io.nr23 = val; return;
    case 0xFF19: gb_state->regs.io.nr24 = val; return;
    case 0xFF1A: gb_state->regs.io.nr30 = val; return;
    case 0xFF1B: gb_state->regs.io.nr31 = val; return;
    case 0xFF1C: gb_state->regs.io.nr32 = val; return;
    case 0xFF1D: gb_state->regs.io.nr33 = val; return;
    case 0xFF1E: gb_state->regs.io.nr34 = val; return;
    case 0xFF20: gb_state->regs.io.nr41 = val; return;
    case 0xFF21: gb_state->regs.io.nr42 = val; return;
    case 0xFF22: gb_state->regs.io.nr43 = val; return;
    case 0xFF23: gb_state->regs.io.nr44 = val; return;
    case 0xFF24: gb_state->regs.io.nr50 = val; return;
    case 0xFF25: gb_state->regs.io.nr51 = val; return;
    case 0xFF0F: gb_state->regs.io.if_ = val; return;
    case 0xFFFF: gb_state->regs.io.ie = val; return;
    case IO_SND_ON: gb_state->regs.io.nr52 = val; return;
    case IO_LCDC: gb_state->regs.io.lcd_control = val; return;
    case IO_SERIAL_TRANSFER:
      // TODO: This just logs out every character written to this port. If I
      // actually want to implement gamelink support there is more to do.
      if (gb_state->serial_port_output != NULL)
        fputc(val, gb_state->serial_port_output);
      return;
    case IO_SERIAL_CONTROL: return;
    case IO_BGP: gb_state->regs.io.bg_pallete = val; return;
    default: NOT_IMPLEMENTED("IO Reg Not Implemented");
    }
  }
  uint8_t *val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
  *val_ptr = val;
}
static inline void write_mem16(struct gb_state *gb_state, uint16_t addr,
                               uint16_t val) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION,
               "Writing val 0x%.4X to address 0x%.4X", val, addr);
  // little endian
  uint8_t *val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
  val_ptr[0] = (val & 0x00FF) >> 0;
  val_ptr[1] = (val & 0xFF00) >> 8;
}

static inline void gb_state_init(struct gb_state *gb_state) {
  SDL_zerop(gb_state);
  // It looks like this was originally at the top of HRAM, but some emulators
  // set SP to the top of WRAM, since I don't have HRAM implemented yet I'm
  // going with the latter approach for now.
  gb_state->regs.sp = WRAM_END + 1;

  // This isn't necessary due to me zeroing state above, but I want to
  // explicitly set this as false in case I ever remove the zeroing as a speed
  // up.
  gb_state->rom_loaded = false;
  gb_state->bootrom_mapped = false;
}
#undef ROM0_START
#undef ROM0_END

#undef ROMN_START
#undef ROMN_END

#undef VRAM_START
#undef VRAM_END

#undef ERAM_START
#undef ERAM_END

#undef WRAM_START
#undef WRAM_END

#endif // GB_COMMON_H
