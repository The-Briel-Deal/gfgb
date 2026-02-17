#include "common.h"

#include <stdint.h>
#include <stdlib.h>

// initialize dynamic string with capacity `cap`
void gb_dstr_init(gb_dstr_t *dstr, size_t cap) {
  dstr->cap = cap;
  dstr->len = 0;
  dstr->txt = GB_malloc(dstr->cap);
}
// free dynamic string
void gb_dstr_free(gb_dstr_t *dstr) {
  dstr->cap = 0;
  dstr->len = 0;
  GB_free(dstr->txt);
  dstr->txt = NULL;
}
// clear dynamic string without freeing or reallocating
void gb_dstr_clear(gb_dstr_t *dstr) { dstr->len = 0; }
// make sure `n` bytes are available after the len of this str
void gb_dstr_ensure_space(gb_dstr_t *dstr, size_t n) {
  size_t req_len = dstr->len + n;
  if (req_len >= dstr->cap) {
    dstr->cap = req_len * 1.5;
    dstr->txt = GB_realloc(dstr->txt, dstr->cap);
  }
}

// append text[len] to gb_dstr
void gb_dstr_append(gb_dstr_t *dstr, char *text, size_t len) {
  gb_dstr_ensure_space(dstr, len);
  SDL_memcpy(&dstr->txt[dstr->len], text, len);
  dstr->len += len;
}

void gb_state_init(struct gb_state *gb_state) {
  SDL_zerop(gb_state);
  // It looks like this was originally at the top of HRAM, but some emulators
  // set SP to the top of WRAM, since I don't have HRAM implemented yet I'm
  // going with the latter approach for now.
  gb_state->regs.sp = WRAM_END + 1;

  // This isn't necessary due to me zeroing state above, but I want to
  // explicitly set this as false in case I ever remove the zeroing as a speed
  // up.
  gb_state->rom_loaded     = false;
  gb_state->bootrom_mapped = false;
  // This is what lcdc is initialized to in neviksti's original disassembly: https://www.neviksti.com/DMG/DMG_ROM.asm
  gb_state->regs.io.lcdc                = 0b10010001;
  gb_state->first_oam_scan_after_enable = true;
}

struct gb_state *gb_state_alloc() { return SDL_malloc(sizeof(struct gb_state)); }

void             gb_state_free(struct gb_state *gb_state) {
  if (gb_state->serial_port_output != NULL) fclose(gb_state->serial_port_output);

  if (gb_state->syms.capacity > 0) {
    free_symbol_list(&gb_state->syms);
  }
  SDL_free(gb_state);
}

uint8_t *get_io_reg(struct gb_state *gb_state, uint16_t addr) {

  GB_assert((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF);
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
  case IO_WY: return &gb_state->regs.io.wy;
  case IO_WX: return &gb_state->regs.io.wx;
  case IO_LYC: return &gb_state->regs.io.lyc;
  case IO_STAT:
    // The least significant 3 bits are RO. I'll need to figure out a way to make sure those bits aren't written to.
    return &gb_state->regs.io.stat;
  case IO_BGP: return &gb_state->regs.io.bgp;
  case IO_OBP0: return &gb_state->regs.io.obp0;
  case IO_OBP1: return &gb_state->regs.io.obp1;
  default: LogError("IO Reg Not Implemented at addr 0x%04X", addr); return NULL;
  }
}
uint8_t get_ro_io_reg(struct gb_state *gb_state, uint16_t addr) {
  (void)gb_state;

  switch (addr) {
  case IO_LY: {
    return gb_state->regs.io.ly;
  }
  default: NOT_IMPLEMENTED("Read Only IO Reg Not Implemented");
  }
}

void *unmap_address(struct gb_state *gb_state, uint16_t addr) {
  if (gb_state->bootrom_mapped && (addr < 0x0100)) {
    return &gb_state->ram.bootrom[addr];
  }
  if (addr <= ROM0_END) {
    return &gb_state->ram.rom0[addr - ROM0_START];
  } else if (addr <= ROMN_END) {
    // TODO: Implement bank switching for this region. For now we'll just assume that it's always 01.
    return &gb_state->ram.rom1[addr - ROMN_START];
  } else if (addr <= VRAM_END) {
    return &gb_state->ram.vram[addr - VRAM_START];
  } else if (addr <= ERAM_END) {
    // TODO: implement eram bank switching
    return &gb_state->ram.eram[addr - ERAM_START];
  } else if (addr <= WRAM_END) {
    return &gb_state->ram.wram[addr - WRAM_START];
  } else if (addr <= ECHO_RAM_END) {
    // Mirrors wram, probably should never be accessed.
    return &gb_state->ram.wram[addr - ECHO_RAM_START];
  } else if (addr <= OAM_END) {
    return &gb_state->ram.oam[addr - OAM_START];
  } else if (addr <= IO_REG_END) {
    goto not_implemented;
  } else if (addr <= HRAM_END) {
    return &gb_state->ram.hram[addr - HRAM_START];
  }
not_implemented:
  LogError("`unmap_address()` was called on an address that is not implemented: 0x%.4X", addr);
  return NULL;
}

// TODO: This and write_mem8() should probably get the gb_ prefix
uint8_t read_mem8(struct gb_state *gb_state, uint16_t addr) {
  if (gb_state->use_flat_ram) {
    return gb_state->flat_ram[addr];
  } else {
    LogTrace("Reading 8 bits from address 0x%.4X", addr);
    if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
      uint8_t val;
      switch (addr) {
      case IO_LY: val = get_ro_io_reg(gb_state, addr); break;
      case IO_STAT:
        // stat is partially read only. we also want to make sure bit 7 is high.
        val = *get_io_reg(gb_state, addr);
        val |= (1 << 7);
        break;
      default:
        uint8_t *io_reg_ptr = get_io_reg(gb_state, addr);
        if (io_reg_ptr == NULL) goto not_implemented;
        val = *io_reg_ptr;
        break;
      }
      LogDebug("Successfully read IO reg at addr = 0x%.4X, val = 0x%.2X", addr, val);
      return val;
    }

    uint8_t *val_ptr = unmap_address(gb_state, addr);
    if (val_ptr != NULL) return *val_ptr;

    goto not_implemented;

  not_implemented:
    LogCritical("`read_mem8()` received a null pointer from unmap_address() when addr = 0x%.4X", addr);
    return 0;
  }
}

uint16_t read_mem16(struct gb_state *gb_state, uint16_t addr) {
  if (gb_state->use_flat_ram) {
    uint8_t *val_ptr = &gb_state->flat_ram[addr];
    uint16_t val     = 0x0000;
    val |= val_ptr[0] << 0;
    val |= val_ptr[1] << 8;
    return val;
  } else {
    LogTrace("Reading 16 bits from address 0x%.4X", addr);
    uint8_t *val_ptr = unmap_address(gb_state, addr);
    if (val_ptr != NULL) {
      uint16_t val = 0x0000;
      val |= val_ptr[0] << 0;
      val |= val_ptr[1] << 8;
      return val;
    } else {
      LogCritical("`read_mem16()` received a null pointer from unmap_address() when addr = 0x%04x", addr);
      return 0;
    }
  }
}
void mark_dirty(struct gb_state *gb_state, uint16_t addr) {
  if (addr >= GB_TILEDATA_BLOCK0_START && addr < GB_TILEDATA_BLOCK2_END) {
    uint16_t tex_idx                  = tile_addr_to_tex_idx(addr);
    gb_state->dirty_textures[tex_idx] = true;
  }
}

void write_io_reg(struct gb_state *gb_state, enum io_reg_addr reg, uint8_t val) {
  // Some IO registers require special handling, like the joypad reg where bit 5 and 4 are read/write, while 3-0 are
  // read-only.
  LogDebug("Writing val = 0x%.2X to IO Reg at addr = 0x%.4X", val, reg);
  switch (reg) {
  case IO_JOYP:
    gb_state->regs.io.joyp &= ~(JOYP_SELECT_BUTTONS | JOYP_SELECT_D_PAD);
    gb_state->regs.io.joyp |= (val & (JOYP_SELECT_BUTTONS | JOYP_SELECT_D_PAD));
    break;
  default:
    uint8_t *reg_ptr = get_io_reg(gb_state, reg);
    if (reg_ptr == NULL) {
      LogError("Unknown IO Register at addr = 0x%.4X", reg);
    }
    *reg_ptr = val;
    break;
  }
}

void write_mem8(struct gb_state *gb_state, uint16_t addr, uint8_t val) {
  if (gb_state->use_flat_ram) {
    gb_state->flat_ram[addr] = val;
  } else {
    LogTrace("Writing val 0x%.2X to address 0x%.4X", val, addr);
    if (addr == IO_SB) {
      // TODO: This just logs out every character written to this port. If I
      // actually want to implement gamelink support there is more to do.
      if (gb_state->serial_port_output != NULL) fputc(val, gb_state->serial_port_output);
      return;
    }
    uint8_t *val_ptr;
    if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
      write_io_reg(gb_state, addr, val);
      return;
    } else {
      val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
    }
    if (val_ptr != NULL) {
      // VRAM is the only place where stuff needs to be uploaded to the GPU (which is expensive). So we mark modified
      // items in vram as dirty where necessary.
      if (addr >= VRAM_START && addr <= VRAM_END) {
        mark_dirty(gb_state, addr);
      }
      *val_ptr = val;
    } else {
      LogCritical("`write_mem8()` received a null pointer from unmap_address() when addr = 0x%04x", addr);
    }
  }
}

void write_mem16(struct gb_state *gb_state, uint16_t addr, uint16_t val) {
  if (gb_state->use_flat_ram) {
    uint8_t *val_ptr = &gb_state->flat_ram[addr];
    val_ptr[0]       = (val & 0x00FF) >> 0;
    val_ptr[1]       = (val & 0xFF00) >> 8;
  } else {
    LogTrace("Writing val 0x%.4X to address 0x%.4X", val, addr);
    // little endian
    uint8_t *val_ptr = ((uint8_t *)unmap_address(gb_state, addr));
    if (val_ptr != NULL) {
      val_ptr[0] = (val & 0x00FF) >> 0;
      val_ptr[1] = (val & 0xFF00) >> 8;
    } else {
      LogCritical("`write_mem16()` received a null pointer from unmap_address() when addr = 0x%04x", addr);
    }
  }
}

uint64_t        m_cycles(struct gb_state *gb_state) { return gb_state->m_cycles_elapsed; }

static uint64_t gb_dots(uint64_t m_cycles) {
  // There are 4 dots per m cycle in dmg normal speed mode, but 2 in cgb double speed mode. If I implement cgb support
  // i'll need to handle that when getting dots.
  return m_cycles * 4;
}

static void update_tima(struct gb_state *gb_state, uint64_t prev_m_cycles, uint64_t curr_m_cycles) {
  uint8_t tac = gb_state->regs.io.tac;
  if ((tac & 0b0100) == 0) return;
  uint16_t incr_every;
  switch (tac & 0b0011) {
  case 0: incr_every = 256; break;
  case 1: incr_every = 4; break;
  case 2: incr_every = 16; break;
  case 3: incr_every = 64; break;
  default: unreachable();
  }
  // we want to floor this to the lowest multiple of the increment rate, that way we don't risk missing increments if
  // this is called too frequently.
  curr_m_cycles /= incr_every;
  curr_m_cycles *= incr_every;

  prev_m_cycles /= incr_every;
  prev_m_cycles *= incr_every;

  uint8_t incr_by = (curr_m_cycles - prev_m_cycles) / incr_every;

  if (((uint32_t)gb_state->regs.io.tima + incr_by) > 0xFF) {
    gb_state->regs.io.if_ |= 0b00100;
  }
  gb_state->regs.io.tima += incr_by;
}

#define DOTS_PER_LINE   456
#define LINES_PER_FRAME 153

static bool lcd_interrupt_triggered(const struct gb_state *gb_state) {

  uint8_t stat       = gb_state->regs.io.stat;
  uint8_t mode       = (stat & (0b11 << 0)) >> 0;
  uint8_t lyc_eq_ly  = (stat & (0b1 << 2)) >> 2;
  uint8_t m0_select  = (stat & (0b1 << 3)) >> 3;
  uint8_t m1_select  = (stat & (0b1 << 4)) >> 4;
  uint8_t m2_select  = (stat & (0b1 << 5)) >> 5;
  uint8_t lyc_select = (stat & (0b1 << 6)) >> 6;

  switch (mode) {
  case HBLANK:
    if (m0_select) return true;
    break;
  case VBLANK:
    if (m1_select) return true;
    break;
  case OAM_SCAN:
    if (m2_select) return true;
    break;
  case DRAWING_PIXELS: break;
  }

  if (lyc_select && lyc_eq_ly) return true;

  return false;
}

static void update_lcd_status(struct gb_state *gb_state, uint64_t prev_m_cycles, uint64_t curr_m_cycles) {
  // Don't update anything besides clearing ppu mode and resetting ly and lx when PPU is disabled.
  if ((gb_state->regs.io.lcdc & (1 << 7)) == 0) {
    // PPU mode reports 0 when PPU is disabled.
    // https://gbdev.io/pandocs/STAT.html#ff41--stat-lcd-status
    gb_state->regs.io.stat &= ~0b0000'0011;
    gb_state->regs.io.ly                  = 0;
    gb_state->lcd_x                       = 0;
    gb_state->first_oam_scan_after_enable = true;
    return;
  }
  uint64_t prev_dots    = gb_dots(prev_m_cycles);
  uint64_t curr_dots    = gb_dots(curr_m_cycles);
  uint32_t dots_elapsed = curr_dots - prev_dots;
  gb_state->lcd_x += dots_elapsed;
  gb_state->regs.io.ly += (gb_state->lcd_x / DOTS_PER_LINE);
  gb_state->lcd_x %= DOTS_PER_LINE;
  gb_state->regs.io.ly %= LINES_PER_FRAME;

  // TODO: we need to see if the previous state already triggered an interupt, if there was already an interrupt being
  // triggered then we don't trigger another one.
  uint8_t mode;
  if (gb_state->regs.io.ly >= 144) {
    mode = VBLANK;
  } else if (gb_state->lcd_x < 80) {
    if (gb_state->first_oam_scan_after_enable)
      mode = (gb_state->regs.io.stat & 0b11);
    else
      mode = OAM_SCAN;
  } else if (gb_state->lcd_x < 369) {
    gb_state->first_oam_scan_after_enable = false;
    // We're assuming that mode 3 is always taking the longest possible amount of time.
    // If we wanted to be really precise we would have to calculate the exact length with:
    // https://gbdev.io/pandocs/Rendering.html#obj-penalty-algorithm
    mode = DRAWING_PIXELS;
  } else {
    mode = HBLANK;
  }
  gb_state->regs.io.stat &= ~0b0000'0011;
  gb_state->regs.io.stat |= mode;

  if (gb_state->regs.io.ly == gb_state->regs.io.lyc) {
    gb_state->regs.io.stat |= 0b0000'0100;
  } else {
    gb_state->regs.io.stat &= ~0b0000'0100;
  }
  bool prev_triggered           = gb_state->last_stat_interrupt;
  bool curr_triggered           = lcd_interrupt_triggered(gb_state);
  gb_state->last_stat_interrupt = curr_triggered;
  if ((!prev_triggered) && curr_triggered) {
    gb_state->regs.io.if_ |= 0b00010;
  }
  if (mode == VBLANK) gb_state->regs.io.if_ |= 0b00001;
}

void update_timers(struct gb_state *gb_state) {
  TracyCZoneN(ctx, "Update Timers", true);
  uint64_t curr_m_cycles             = m_cycles(gb_state);
  uint64_t prev_m_cycles             = gb_state->last_timer_sync_m_cycles;
  gb_state->last_timer_sync_m_cycles = curr_m_cycles;

  update_tima(gb_state, prev_m_cycles, curr_m_cycles);
  update_lcd_status(gb_state, prev_m_cycles, curr_m_cycles);
  TracyCZoneEnd(ctx);
}

bool gb_state_get_err(struct gb_state *gb_state) {
  bool err      = gb_state->err;
  gb_state->err = false;
  return err;
}

void gb_state_use_flat_mem(struct gb_state *gb_state, bool enabled) { gb_state->use_flat_ram = enabled; }
