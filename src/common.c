#include "common.h"

void gb_state_init(struct gb_state *gb_state) {
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

struct gb_state *gb_state_alloc() { return SDL_malloc(sizeof(struct gb_state)); }

void gb_state_free(struct gb_state *gb_state) {
  if (gb_state->serial_port_output != NULL) fclose(gb_state->serial_port_output);

  if (gb_state->syms.capacity > 0) {
    free_symbol_list(&gb_state->syms);
  }
  SDL_free(gb_state);
}

static uint8_t *get_io_reg(struct gb_state *gb_state, uint16_t addr) {
  assert((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF);
  switch (addr) {
  case IO_SB: NOT_IMPLEMENTED("Actual IO_SERIAL_TRANSFER reg not implemented.");
  case IO_SC: return &gb_state->regs.io.sc;
  case IO_TIMA: return &gb_state->regs.io.tima;
  case IO_TMA: return &gb_state->regs.io.tma;
  case IO_TAC: return &gb_state->regs.io.tac;
  case IO_NR10: return &gb_state->regs.io.nr10;
  case IO_NR11: return &gb_state->regs.io.nr11;
  case IO_NR12: return &gb_state->regs.io.nr12;
  case IO_NR13: return &gb_state->regs.io.nr13;
  case IO_NR14: return &gb_state->regs.io.nr14;
  case IO_NR21: return &gb_state->regs.io.nr21;
  case IO_NR22: return &gb_state->regs.io.nr22;
  case IO_NR23: return &gb_state->regs.io.nr23;
  case IO_NR24: return &gb_state->regs.io.nr24;
  case IO_NR30: return &gb_state->regs.io.nr30;
  case IO_NR31: return &gb_state->regs.io.nr31;
  case IO_NR32: return &gb_state->regs.io.nr32;
  case IO_NR33: return &gb_state->regs.io.nr33;
  case IO_NR34: return &gb_state->regs.io.nr34;
  case IO_NR41: return &gb_state->regs.io.nr41;
  case IO_NR42: return &gb_state->regs.io.nr42;
  case IO_NR43: return &gb_state->regs.io.nr43;
  case IO_NR44: return &gb_state->regs.io.nr44;
  case IO_NR50: return &gb_state->regs.io.nr50;
  case IO_NR51: return &gb_state->regs.io.nr51;
  case IO_IF: return &gb_state->regs.io.if_;
  case IO_IE: return &gb_state->regs.io.ie;
  case IO_SND_ON: return &gb_state->regs.io.nr52;
  case IO_LCDC: return &gb_state->regs.io.lcdc;
  case IO_SCY: return &gb_state->regs.io.scy;
  case IO_SCX: return &gb_state->regs.io.scx;
  case IO_BGP: return &gb_state->regs.io.bg_pallete;
  default: NOT_IMPLEMENTED("IO Reg Not Implemented");
  }
}
// For the read only IO Reg's which are computed lazily.
static uint8_t get_ro_io_reg(struct gb_state *gb_state, uint16_t addr) {
  (void)gb_state;

  switch (addr) {
  case IO_LY: {
    uint32_t dots = gb_dots();
    dots %= DOTS_PER_FRAME;
    uint8_t ly = dots / 456;
    assert(ly < 154);
    return ly;
  }
  default: NOT_IMPLEMENTED("Read Only IO Reg Not Implemented");
  }
}

void *unmap_address(struct gb_state *gb_state, uint16_t addr) {
  if (gb_state->bootrom_mapped && (addr < 0x0100)) {
    return &gb_state->bootrom[addr];
  }
  if (addr <= ROM0_END) {
    return &gb_state->rom0[addr - ROM0_START];
  } else if (addr <= ROMN_END) {
    // TODO: Implement bank switching for this region. For now we'll just assume that it's always 01.
    return &gb_state->rom1[addr - ROMN_START];
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

uint8_t read_mem8(struct gb_state *gb_state, uint16_t addr) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION, "Reading 8 bits from address 0x%.4X", addr);
  if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
    switch (addr) {
    case IO_LY: return get_ro_io_reg(gb_state, addr);
    default: return *get_io_reg(gb_state, addr);
    }
  }

  uint8_t val = *((uint8_t *)unmap_address(gb_state, addr));
  return val;
}

uint16_t read_mem16(struct gb_state *gb_state, uint16_t addr) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION, "Reading 16 bits from address 0x%.4X", addr);
  uint8_t *val_ptr = unmap_address(gb_state, addr);
  uint16_t val = 0x0000;
  val |= val_ptr[0] << 0;
  val |= val_ptr[1] << 8;
  return val;
}

void write_mem8(struct gb_state *gb_state, uint16_t addr, uint8_t val) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION, "Writing val 0x%.2X to address 0x%.4X", val, addr);
  if (addr == IO_SB) {
    // TODO: This just logs out every character written to this port. If I
    // actually want to implement gamelink support there is more to do.
    if (gb_state->serial_port_output != NULL) fputc(val, gb_state->serial_port_output);
    return;
  }
  uint8_t *val_ptr;
  if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
    val_ptr = get_io_reg(gb_state, addr);
  } else {
    val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
  }
  *val_ptr = val;
}

void write_mem16(struct gb_state *gb_state, uint16_t addr, uint16_t val) {
  SDL_LogTrace(SDL_LOG_CATEGORY_APPLICATION, "Writing val 0x%.4X to address 0x%.4X", val, addr);
  // little endian
  uint8_t *val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
  val_ptr[0] = (val & 0x00FF) >> 0;
  val_ptr[1] = (val & 0xFF00) >> 8;
}
