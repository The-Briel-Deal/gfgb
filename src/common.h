#ifndef GB_COMMON_H
#define GB_COMMON_H

#include <SDL3/SDL.h>
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

#define KB(n)             (1024 * n)

#define NIBBLE0(byte)     ((byte & (0xF0 >> 0)) >> 4)
#define NIBBLE1(byte)     ((byte & (0xF0 >> 4)) >> 0)

#define CRUMB0(byte)      ((byte & (0b11000000 >> 0)) >> 6)
#define CRUMB1(byte)      ((byte & (0b11000000 >> 2)) >> 4)
#define CRUMB2(byte)      ((byte & (0b11000000 >> 4)) >> 2)
#define CRUMB3(byte)      ((byte & (0b11000000 >> 6)) >> 0)

#define GB_DISPLAY_WIDTH  160
#define GB_DISPLAY_HEIGHT 144

#define NS_PER_SEC        (1 * 1000 * 1000 * 1000)

#define DMG_CLOCK_HZ      (1 << 22)

#define DOTS_PER_FRAME    70224

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
  struct {
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
      uint8_t sound_on;
      uint8_t lcd_control;
    } io;
  } regs;
  uint8_t rom0[KB(16)];
  uint8_t wram[KB(8)];
  uint8_t vram[KB(8)];
  uint8_t display[GB_DISPLAY_WIDTH][GB_DISPLAY_HEIGHT];
};

#define ROM0_START 0x0000
#define ROM0_END   0x3FFF

#define ROMN_START 0x4000
#define ROMN_END   0x7FFF

// VRAM on CGB is switchable accross 2 8KB banks, on DMG this is just one 8KB
// block. I won't worry about this until DMG is finished.
#define VRAM_START 0x8000
#define VRAM_END   0x9FFF

#define ERAM_START 0xA000
#define ERAM_END   0xBFFF

// This is split in two on the CGB and the second half is switchable. I'm just
// worrying about DMG for now.
#define WRAM_START 0xC000
#define WRAM_END   0xDFFF
static void *unmap_address(struct gb_state *gb_state, uint16_t addr) {
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

#define IO_REG_START 0xFF00
#define IO_REG_END   0xFF7F

#define IO_SND_ON    0xFF26
#define IO_LCDC      0xFF40
#define IO_LY        0xFF44

static inline uint8_t read_mem8(struct gb_state *gb_state, uint16_t addr) {
  uint8_t val;
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
  val = *((uint8_t *)unmap_address(gb_state, addr));
  return val;
}

static inline uint16_t read_mem16(struct gb_state *gb_state, uint16_t addr) {
  uint8_t *val_ptr = unmap_address(gb_state, addr);
  uint16_t val = 0x0000;
  val |= val_ptr[0] << 0;
  val |= val_ptr[1] << 8;
  return val;
}

static inline void write_mem8(struct gb_state *gb_state, uint16_t addr,
                              uint8_t val) {
  if (addr >= IO_REG_START && addr <= IO_REG_END) {
    switch (addr) {
    case IO_SND_ON: {
      gb_state->regs.io.sound_on = val;
      return;
    }
    case IO_LCDC: {
      gb_state->regs.io.lcd_control = val;
      return;
    }
    default: NOT_IMPLEMENTED("IO Reg Not Implemented");
    }
  }
  uint8_t *val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
  *val_ptr = val;
}
static inline void write_mem16(struct gb_state *gb_state, uint16_t addr,
                               uint16_t val) {
  // little endian
  uint8_t *val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
  val_ptr[0] = (val & 0x00FF) >> 0;
  val_ptr[1] = (val & 0xFF00) >> 8;
}

static inline void gb_state_init(struct gb_state *gb_state) {
  SDL_zerop(gb_state);
  // In reality the pc should be initialized to 0x0000 where the boot rom
  // starts, but practically it's fine to just skip the boot rom and start at
  // our programs location at 0x0100.
  gb_state->regs.pc = 0x0100;
  // It looks like this was originally at the top of HRAM, but some emulators
  // set SP to the top of WRAM, since I don't have HRAM implemented yet I'm
  // going with the latter approach for now.
  gb_state->regs.sp = WRAM_END;
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
