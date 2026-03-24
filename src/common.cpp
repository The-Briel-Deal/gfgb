#include "common.h"
#include "ppu.h"
#include "timing.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX
#include "incbin.h"
INCBIN(dmg0_boot_rom, "bootroms/dmg0_boot.bin");

#define GB_HEADER_CART_TYPE_ADDR 0x0147

gb_cart_header_t gb_parse_cart_header(uint8_t *header[0x50]) {
  gb_cart_header_t parsed_header;

  uint8_t cart_type = (*header)[GB_HEADER_CART_TYPE_ADDR - 0x100];
  switch (cart_type) {
  case 0x00: parsed_header = {GB_NO_MBC, false, false, false, false}; break; // 00h  ROM ONLY
  case 0x01: parsed_header = {GB_MBC1, false, false, false, false}; break;   // 01h  MBC1
  case 0x02: parsed_header = {GB_MBC1, true, false, false, false}; break;    // 02h  MBC1+RAM
  case 0x03: parsed_header = {GB_MBC1, true, true, false, false}; break;     // 03h  MBC1+RAM+BATTERY
  case 0x05: parsed_header = {GB_MBC2, true, false, false, false}; break;    // 05h  MBC2
  case 0x06: parsed_header = {GB_MBC2, true, true, false, false}; break;     // 06h  MBC2+BATTERY
  case 0x08: parsed_header = {GB_NO_MBC, true, false, false, false}; break;  // 08h  ROM+RAM
  case 0x09: parsed_header = {GB_NO_MBC, true, true, false, false}; break;   // 09h  ROM+RAM+BATTERY
  case 0x0B: parsed_header = {GB_MMM01, false, false, false, false}; break;  // 0Bh  MMM01
  case 0x0C: parsed_header = {GB_MMM01, true, false, false, false}; break;   // 0Ch  MMM01+RAM
  case 0x0D: parsed_header = {GB_MMM01, true, true, false, false}; break;    // 0Dh  MMM01+RAM+BATTERY
  case 0x0F: parsed_header = {GB_MBC3, false, true, true, false}; break;     // 0Fh  MBC3+TIMER+BATTERY
  case 0x10: parsed_header = {GB_MBC3, true, true, true, false}; break;      // 10h  MBC3+TIMER+RAM+BATTERY
  case 0x11: parsed_header = {GB_MBC3, false, false, false, false}; break;   // 11h  MBC3
  case 0x12: parsed_header = {GB_MBC3, true, false, false, false}; break;    // 12h  MBC3+RAM
  case 0x13: parsed_header = {GB_MBC3, true, true, false, false}; break;     // 13h  MBC3+RAM+BATTERY
  case 0x19: parsed_header = {GB_MBC5, false, false, false, false}; break;   // 19h  MBC5
  case 0x1A: parsed_header = {GB_MBC5, true, false, false, false}; break;    // 1Ah  MBC5+RAM
  case 0x1B: parsed_header = {GB_MBC5, true, true, false, false}; break;     // 1Bh  MBC5+RAM+BATTERY
  case 0x1C: parsed_header = {GB_MBC5, false, false, false, true}; break;    // 1Ch  MBC5+RUMBLE
  case 0x1D: parsed_header = {GB_MBC5, true, false, false, true}; break;     // 1Dh  MBC5+RUMBLE+RAM
  case 0x1E: parsed_header = {GB_MBC5, true, true, false, true}; break;      // 1Eh  MBC5+RUMBLE+RAM+BATTERY
  case 0x22: parsed_header = {GB_MBC7, true, true, false, false}; break;     // 22h  MBC7+ACCEL+EEPROM
  case 0xFC: parsed_header = {GB_CAMERA, true, true, false, false}; break;   // FCh  POCKET CAMERA
  case 0xFD: parsed_header = {GB_NO_MBC, false, false, false, false}; break; // FDh  BANDAI TAMA5 (Todo: Not supported)
  case 0xFE: parsed_header = {GB_HUC3, true, true, true, false}; break;      // FEh  HuC3
  case 0xFF: parsed_header = {GB_HUC1, true, true, false, false}; break;     // FFh  HuC1+RAM+BATTERY
  default:
    LogWarn("`gb_parse_cart_header() called an unknown cart type [$0147]`");
    parsed_header = {GB_MBC_UNKNOWN, false, false, false, false};
    break;
  }
  return parsed_header;
};

// initialize dynamic string with capacity `cap`
void gb_dstr_init(gb_dstr_t *dstr, size_t cap) {
  dstr->cap = cap;
  dstr->len = 0;
  dstr->txt = (char *)GB_malloc(dstr->cap);
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
    dstr->txt = (char *)GB_realloc(dstr->txt, dstr->cap);
  }
}

// append text[len] to gb_dstr
void gb_dstr_append(gb_dstr_t *dstr, char *text, size_t len) {
  gb_dstr_ensure_space(dstr, len);
  SDL_memcpy(&dstr->txt[dstr->len], text, len);
  dstr->len += len;
}

void gb_state_init(struct gb_state *gb_state) {
  // We only manually initialize non-zero vals since we zero gb_state first.
  GB_memset(gb_state, 0, sizeof(*gb_state));
  /// Registers
  // It looks like this was originally at the top of HRAM, but some emulators
  // set SP to the top of WRAM, since I don't have HRAM implemented yet I'm
  // going with the latter approach for now.
  gb_state->saved.regs.sp      = WRAM_END + 1;
  gb_state->saved.regs.io.lcdc = 0b1001'0001;
  gb_state->saved.regs.io.sc   = 0b0111'1110;

  /// Internal State
  gb_state->video.first_oam_scan_after_enable = true;

  // Debug State
  gb_state->dbg.speed_factor          = 1.0;
  gb_state->breakpoints               = new std::vector<gb_breakpoint_t>;
  gb_state->serial_port_output_string = new std::string;
  gb_state->dbg.execution_paused      = false;
  gb_state->video.initialized         = false;
}

// TODO: This doesn't seem to always work, I need to figure out what other state I need set.
void gb_state_reset(struct gb_state *gb_state) {
  GB_memset(&gb_state->saved.regs, 0, sizeof(gb_state->saved.regs));
  gb_state->saved.regs.sp                     = WRAM_END + 1;
  gb_state->saved.regs.pc                     = 0;
  gb_state->saved.regs.io.bank                = true;
  gb_state->video.first_oam_scan_after_enable = true;
  gb_state->saved.m_cycles_elapsed            = 0;
  gb_state->saved.last_timer_sync_m_cycles    = 0;
  gb_state->timing.ns_elapsed_while_running   = 0;
}

void gb_state_load_bootrom(struct gb_state *gb_state, const char *bootrom_name) {
  // Load bootrom into gb_state->bootrom (bootrom is optional)
  if (bootrom_name != NULL) {
    FILE *f;
    int   bytes_len;
    int   err;
    f         = fopen(bootrom_name, "r");
    bytes_len = fread(gb_state->saved.ram.bootrom, sizeof(uint8_t), 0x0100, f);
    if ((err = ferror(f))) {
      LogError("Error when reading bootrom file: %d", err);
      goto load_default;
    }
    fclose(f);
    GB_assert(bytes_len == 0x0100);
    gb_state->saved.regs.pc      = 0x0000;
    gb_state->saved.regs.io.bank = true;
  }
load_default:
  GB_assert(dmg0_boot_rom_size == 0x0100);
  memcpy(gb_state->saved.ram.bootrom, dmg0_boot_rom_data, 0x0100);
  gb_state->saved.regs.pc      = 0x0000;
  gb_state->saved.regs.io.bank = true;
}

struct gb_state *gb_state_alloc() { return (gb_state *)GB_malloc(sizeof(struct gb_state)); }

void gb_state_free(struct gb_state *gb_state) {
  if (gb_state->dbg.serial_port_output_file != NULL) fclose(gb_state->dbg.serial_port_output_file);

  if (gb_state->dbg.syms.capacity > 0) {
    free_symbol_list(&gb_state->dbg.syms);
  }
  delete gb_state->breakpoints;
  delete gb_state->serial_port_output_string;
  delete gb_state->compiled_pass_regex;
  delete gb_state->compiled_fail_regex;
  GB_free(gb_state);
}

uint8_t *get_io_reg(struct gb_state *gb_state, uint16_t addr) {

  GB_assert((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF);
  switch (addr) {
  case IO_JOYP: return &gb_state->saved.regs.io.joyp;
  case IO_SB:
    return &gb_state->saved.regs.io.sb; // Actual reg not used. I currently just write this to a file for logging.
  case IO_SC: return &gb_state->saved.regs.io.sc;
  case IO_TIMA: return &gb_state->saved.regs.io.tima;
  case IO_TMA: return &gb_state->saved.regs.io.tma;
  case IO_TAC: return &gb_state->saved.regs.io.tac;
  case IO_NR10: return &gb_state->saved.regs.io.nr10;
  case IO_NR11: return &gb_state->saved.regs.io.nr11;
  case IO_NR12: return &gb_state->saved.regs.io.nr12;
  case IO_NR13: return &gb_state->saved.regs.io.nr13;
  case IO_NR14: return &gb_state->saved.regs.io.nr14;
  case IO_NR21: return &gb_state->saved.regs.io.nr21;
  case IO_NR22: return &gb_state->saved.regs.io.nr22;
  case IO_NR23: return &gb_state->saved.regs.io.nr23;
  case IO_NR24: return &gb_state->saved.regs.io.nr24;
  case IO_NR30: return &gb_state->saved.regs.io.nr30;
  case IO_NR31: return &gb_state->saved.regs.io.nr31;
  case IO_NR32: return &gb_state->saved.regs.io.nr32;
  case IO_NR33: return &gb_state->saved.regs.io.nr33;
  case IO_NR34: return &gb_state->saved.regs.io.nr34;
  case IO_NR41: return &gb_state->saved.regs.io.nr41;
  case IO_NR42: return &gb_state->saved.regs.io.nr42;
  case IO_NR43: return &gb_state->saved.regs.io.nr43;
  case IO_NR44: return &gb_state->saved.regs.io.nr44;
  case IO_NR50: return &gb_state->saved.regs.io.nr50;
  case IO_NR51: return &gb_state->saved.regs.io.nr51;
  case IO_IF: return &gb_state->saved.regs.io.if_;
  case IO_IE: return &gb_state->saved.regs.io.ie;
  case IO_DMA: return &gb_state->saved.regs.io.dma;
  case IO_SND_ON: return &gb_state->saved.regs.io.nr52;
  case IO_LCDC: return &gb_state->saved.regs.io.lcdc;
  case IO_SCY: return &gb_state->saved.regs.io.scy;
  case IO_SCX: return &gb_state->saved.regs.io.scx;
  case IO_WY: return &gb_state->saved.regs.io.wy;
  case IO_WX: return &gb_state->saved.regs.io.wx;
  case IO_LYC: return &gb_state->saved.regs.io.lyc;
  case IO_STAT: return &gb_state->saved.regs.io.stat; // TODO: Lower 3 bits need to be RO.
  case IO_BGP: return &gb_state->saved.regs.io.bgp;
  case IO_OBP0: return &gb_state->saved.regs.io.obp0;
  case IO_OBP1: return &gb_state->saved.regs.io.obp1;
  default: ERR(gb_state, "IO Reg Not Implemented at addr 0x%04X", addr); return NULL;
  }
}
uint8_t get_ro_io_reg(struct gb_state *gb_state, uint16_t addr) {
  (void)gb_state;

  switch (addr) {
  case IO_LY: {
    return gb_state->saved.regs.io.ly;
  }
  case IO_DIV: {
    // Internally div is a 16 bit register but only the most significant 8 bits are mapped to mem.
    return gb_state->saved.regs.io.div;
  }
  default: NOT_IMPLEMENTED("Read Only IO Reg Not Implemented");
  }
}

void *gb_unmap_address(struct gb_state *gb_state, uint16_t addr) {
  if (gb_state->saved.regs.io.bank && (addr < 0x0100)) {
    return &gb_state->saved.ram.bootrom[addr];
  }
  if (addr <= ROM0_END) {
    return &gb_state->saved.ram.rom0[addr - ROM0_START];
  } else if (addr <= ROMN_END) {
    // TODO: Implement bank switching for this region. For now we'll just assume that it's always 01.
    return &gb_state->saved.ram.rom1[addr - ROMN_START];
  } else if (addr <= VRAM_END) {
    return &gb_state->saved.ram.vram[addr - VRAM_START];
  } else if (addr <= ERAM_END) {
    // TODO: implement eram bank switching
    return &gb_state->saved.ram.eram[addr - ERAM_START];
  } else if (addr <= WRAM_END) {
    return &gb_state->saved.ram.wram[addr - WRAM_START];
  } else if (addr <= ECHO_RAM_END) {
    // Mirrors wram, probably should never be accessed.
    return &gb_state->saved.ram.wram[addr - ECHO_RAM_START];
  } else if (addr <= OAM_END) {
    return &gb_state->saved.ram.oam[addr - OAM_START];
  } else if (addr <= IO_REG_END) {
    goto not_implemented;
  } else if (addr <= HRAM_END) {
    return &gb_state->saved.ram.hram[addr - HRAM_START];
  }
not_implemented:
  LogError("`gb_unmap_address()` was called on an address that is not implemented: 0x%.4X", addr);
  return NULL;
}

uint8_t gb_read_mem(struct gb_state *gb_state, uint16_t addr) {
  uint8_t *val_ptr;
  if (gb_state->dbg.use_flat_ram) {
    return gb_state->saved.flat_ram[addr];
  } else {
    LogTrace("Reading 8 bits from address 0x%.4X", addr);
    if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
      uint8_t val;
      switch (addr) {
      case IO_LY: val = get_ro_io_reg(gb_state, addr); break;
      case IO_DIV: {
        val = get_ro_io_reg(gb_state, addr);
        break;
      }
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
      LogDebugCat(GB_LOG_CATEGORY_IO_REGS, "Successfully read IO reg at addr = 0x%.4X, val = 0x%.2X", addr, val);
      return val;
    }

    val_ptr = (uint8_t *)gb_unmap_address(gb_state, addr);
    if (val_ptr != NULL) return *val_ptr;

    goto not_implemented;

  not_implemented:
    // It isn't always a critical issue to read unused mem/io-regs, tetris seems to do it unintentionally.
    LogWarn("`read_mem()` received a null pointer from `gb_unmap_address()` when addr = 0x%.4X", addr);
    return 0xFF;
  }
}

void mark_dirty(struct gb_state *gb_state, uint16_t addr) {
  if (addr >= GB_TILEDATA_BLOCK0_START && addr < GB_TILEDATA_BLOCK2_END) {
    uint16_t tex_idx                        = gb_tile_addr_to_tex_idx(addr);
    gb_state->video.dirty_textures[tex_idx] = true;
  }
}

static void write_io_reg(struct gb_state *gb_state, io_reg_addr_t reg, uint8_t val) {
  // Some IO registers require special handling, like the joypad reg where bit 5 and 4 are read/write, while 3-0 are
  // read-only.
  LogDebugCat(GB_LOG_CATEGORY_IO_REGS, "Writing val = 0x%.2X to IO Reg at addr = 0x%.4X", val, reg);
  switch (reg) {
  case IO_SC:
    // TODO: CGB uses bit 1 for clock speed. I'll need to implement that if I add CGB support.
    *get_io_reg(gb_state, IO_SC) = val | 0b0111'1110;
    break;
  case IO_JOYP:
    gb_state->saved.regs.io.joyp &= ~(JOYP_SELECT_BUTTONS | JOYP_SELECT_D_PAD);
    gb_state->saved.regs.io.joyp |= (val & (JOYP_SELECT_BUTTONS | JOYP_SELECT_D_PAD));
    break;
  case IO_DIV: gb_handle_div_write(gb_state); break;
  case IO_BANK:
    // if bit 0 is set unmap bootrom. This can't be re-enabled without a restart.
    if (val & 1) {
      gb_state->saved.regs.io.bank = false;
    }
    break;
  default:
    if (reg == IO_DMA) {
      gb_state->video.oam_dma_start = true;
    }
    uint8_t *reg_ptr = get_io_reg(gb_state, reg);
    if (reg_ptr == NULL) {
      LogError("Unknown IO Register at addr = 0x%.4X", reg);
      break;
    }
    *reg_ptr = val;
    break;
  }
}
static void gb_write_mbc1(struct gb_state *gb_state, uint16_t addr, uint8_t val) {}

// Called whenever gb_write_mem is called on ROM.
static void gb_write_mbc(struct gb_state *gb_state, uint16_t addr, uint8_t val) {
  uint8_t cart_type = gb_read_mem(gb_state, GB_HEADER_CART_TYPE_ADDR);
  switch (cart_type) {
  case 0x00: // ROM Only
  }
}

void gb_write_mem(struct gb_state *gb_state, uint16_t addr, uint8_t val) {
  if (gb_state->dbg.use_flat_ram) {
    gb_state->saved.flat_ram[addr] = val;
  } else {
    LogTrace("Writing val 0x%.2X to address 0x%.4X", val, addr);
    uint8_t *val_ptr;
    if (addr < 0x4000) {
      // TODO: Everything in the rom section is read only and writes here control the MBC which still needs to be
      // implemented.
      return;
    }
    if ((addr >= IO_REG_START && addr <= IO_REG_END) || addr == 0xFFFF) {
      if (addr == IO_SB) {
        // TODO: This just logs out every character written to this port. If I
        // actually want to implement gamelink support there is more to do.
        if (gb_state->dbg.serial_port_output_file != NULL) fputc(val, gb_state->dbg.serial_port_output_file);
        gb_state->serial_port_output_string->push_back(val);
        return;
      }
      write_io_reg(gb_state, addr, val);
      return;
    } else {
      val_ptr = ((uint8_t *)gb_unmap_address(gb_state, addr));
    }
    if (val_ptr != NULL) {
      // VRAM is the only place where stuff needs to be uploaded to the GPU (which is expensive). So we mark modified
      // items in vram as dirty where necessary.
      if (addr >= VRAM_START && addr <= VRAM_END) {
        mark_dirty(gb_state, addr);
      }
      *val_ptr = val;
    } else {
      LogCritical("`write_mem()` received a null pointer from `gb_unmap_address()` when addr = 0x%04x", addr);
    }
  }
}

uint64_t gb_m_cycles(struct gb_state *gb_state) { return gb_state->saved.m_cycles_elapsed; }

#define DOTS_PER_LINE   456
#define LINES_PER_FRAME 153

bool gb_state_get_err(struct gb_state *gb_state) {
  bool err          = gb_state->dbg.err;
  gb_state->dbg.err = false;
  return err;
}

void gb_state_use_flat_mem(struct gb_state *gb_state, bool enabled) { gb_state->dbg.use_flat_ram = enabled; }
