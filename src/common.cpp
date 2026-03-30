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

#define GB_HEADER_START          0x0100
#define GB_HEADER_SIZE           0x0050
#define GB_HEADER_CART_TYPE_ADDR 0x0147
#define GB_HEADER_ROM_SIZE_ADDR  0x0148
#define GB_HEADER_RAM_SIZE_ADDR  0x0149

#define SET_CART_TYPE(parsed_header, _mbc_type, _has_ram, _has_battery, _has_rtc, _has_rumble)                         \
  {                                                                                                                    \
    parsed_header.mbc_type    = _mbc_type;                                                                             \
    parsed_header.has_ram     = _has_ram;                                                                              \
    parsed_header.has_battery = _has_battery;                                                                          \
    parsed_header.has_rtc     = _has_rtc;                                                                              \
    parsed_header.has_rumble  = _has_rumble;                                                                           \
  }

gb_cart_header_t gb_parse_cart_header(uint8_t header[0x50]) {
  gb_cart_header_t parsed_header;

  uint8_t cart_type = header[GB_HEADER_CART_TYPE_ADDR - 0x100];
  switch (cart_type) {
  case 0x00: SET_CART_TYPE(parsed_header, GB_NO_MBC, false, false, false, false); break; // ROM ONLY
  case 0x01: SET_CART_TYPE(parsed_header, GB_MBC1, false, false, false, false); break;   // MBC1
  case 0x02: SET_CART_TYPE(parsed_header, GB_MBC1, true, false, false, false); break;    // MBC1+RAM
  case 0x03: SET_CART_TYPE(parsed_header, GB_MBC1, true, true, false, false); break;     // MBC1+RAM+BATTERY
  case 0x05: SET_CART_TYPE(parsed_header, GB_MBC2, true, false, false, false); break;    // MBC2
  case 0x06: SET_CART_TYPE(parsed_header, GB_MBC2, true, true, false, false); break;     // MBC2+BATTERY
  case 0x08: SET_CART_TYPE(parsed_header, GB_NO_MBC, true, false, false, false); break;  // ROM+RAM
  case 0x09: SET_CART_TYPE(parsed_header, GB_NO_MBC, true, true, false, false); break;   // ROM+RAM+BATTERY
  case 0x0B: SET_CART_TYPE(parsed_header, GB_MMM01, false, false, false, false); break;  // MMM01
  case 0x0C: SET_CART_TYPE(parsed_header, GB_MMM01, true, false, false, false); break;   // MMM01+RAM
  case 0x0D: SET_CART_TYPE(parsed_header, GB_MMM01, true, true, false, false); break;    // MMM01+RAM+BATTERY
  case 0x0F: SET_CART_TYPE(parsed_header, GB_MBC3, false, true, true, false); break;     // MBC3+TIMER+BATTERY
  case 0x10: SET_CART_TYPE(parsed_header, GB_MBC3, true, true, true, false); break;      // MBC3+TIMER+RAM+BATTERY
  case 0x11: SET_CART_TYPE(parsed_header, GB_MBC3, false, false, false, false); break;   // MBC3
  case 0x12: SET_CART_TYPE(parsed_header, GB_MBC3, true, false, false, false); break;    // MBC3+RAM
  case 0x13: SET_CART_TYPE(parsed_header, GB_MBC3, true, true, false, false); break;     // MBC3+RAM+BATTERY
  case 0x19: SET_CART_TYPE(parsed_header, GB_MBC5, false, false, false, false); break;   // MBC5
  case 0x1A: SET_CART_TYPE(parsed_header, GB_MBC5, true, false, false, false); break;    // MBC5+RAM
  case 0x1B: SET_CART_TYPE(parsed_header, GB_MBC5, true, true, false, false); break;     // MBC5+RAM+BATTERY
  case 0x1C: SET_CART_TYPE(parsed_header, GB_MBC5, false, false, false, true); break;    // MBC5+RUMBLE
  case 0x1D: SET_CART_TYPE(parsed_header, GB_MBC5, true, false, false, true); break;     // MBC5+RUMBLE+RAM
  case 0x1E: SET_CART_TYPE(parsed_header, GB_MBC5, true, true, false, true); break;      // MBC5+RUMBLE+RAM+BATTERY
  case 0x22: SET_CART_TYPE(parsed_header, GB_MBC7, true, true, false, false); break;     // MBC7+ACCEL+EEPROM
  case 0xFC: SET_CART_TYPE(parsed_header, GB_CAMERA, true, true, false, false); break;   // POCKET CAMERA
  case 0xFD: SET_CART_TYPE(parsed_header, GB_NO_MBC, false, false, false, false); break; // BANDAI TAMA5
  case 0xFE: SET_CART_TYPE(parsed_header, GB_HUC3, true, true, true, false); break;      // HuC3
  case 0xFF: SET_CART_TYPE(parsed_header, GB_HUC1, true, true, false, false); break;     // HuC1+RAM+BATTERY
  default:
    LogWarn("`gb_parse_cart_header() called an unknown cart type [$0147]`");
    SET_CART_TYPE(parsed_header, GB_MBC_UNKNOWN, false, false, false, false);
    break;
  }
  uint8_t rom_size = header[GB_HEADER_ROM_SIZE_ADDR - 0x100];
  // https://gbdev.io/pandocs/The_Cartridge_Header.html#weird_rom_sizes
  // Apparently there are a few other valid rom sizes that no real world rom uses. I don't think I need to implement
  // these so I'm just going to add an assert here in case we somehow run into one of these.
  parsed_header.num_rom_banks = (2 << rom_size);

  uint8_t ram_size = header[GB_HEADER_RAM_SIZE_ADDR - 0x100];
  if (ram_size == 0) {
    parsed_header.num_ram_banks = 0;
  } else {
    GB_assert(parsed_header.has_ram);
    // Apparently they had a lil' bank that was 2KiB planned but they never made the chip so this was never
    // actually used in a cartridge. RIP lil' bank, you were gone too soon.
    GB_assert(ram_size != 1);

    if (ram_size == 2) parsed_header.num_ram_banks = 1;
    if (ram_size == 3) parsed_header.num_ram_banks = 4;
    if (ram_size == 4) parsed_header.num_ram_banks = 16; // WTF is this choice of code num to bank count? Why didn't
    if (ram_size == 5) parsed_header.num_ram_banks = 8;  // they just do the same thing they did with rom banks?
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

void gb_alloc_mbc1(gb_state_t *gb_state) {
  const gb_cart_header_t &header = gb_state->saved.header;

  uint32_t mbc_bytes_required = 0;

  uint32_t rom_banks_size = (header.num_rom_banks * (KB(16)));
  mbc_bytes_required += rom_banks_size;
  uint32_t eram_banks_size = (header.num_ram_banks * (KB(8)));
  mbc_bytes_required += eram_banks_size;
  // These are both allocated in one call to malloc, the eram block comes directly after the rom block.
  gb_state->saved.mem.rom_start = (uint8_t *)GB_malloc(mbc_bytes_required);
  gb_state->saved.mem.rom_size  = rom_banks_size;
  GB_assert(gb_state->saved.mem.rom_start != NULL);
  gb_state->saved.mem.eram_start = &gb_state->saved.mem.rom_start[rom_banks_size];
  gb_state->saved.mem.eram_size  = eram_banks_size;

  // The rom_bank is the one field that should default to 1,
  // everything else was initialized to zero in gb_state_init().
  gb_state->saved.regs.mbc1_regs.rom_bank = 1;
}

void gb_alloc_no_mbc(gb_state_t *gb_state) {
  uint32_t mbc_bytes_required = 0;

  uint32_t rom_banks_size = (2 * (KB(16)));
  mbc_bytes_required += rom_banks_size;
  uint32_t eram_banks_size = (1 * (KB(8)));
  mbc_bytes_required += eram_banks_size;
  // These are both allocated in one call to malloc, the eram block comes directly after the rom block.
  gb_state->saved.mem.rom_start = (uint8_t *)GB_malloc(mbc_bytes_required);
  gb_state->saved.mem.rom_size  = rom_banks_size;
  GB_assert(gb_state->saved.mem.rom_start != NULL);
  gb_state->saved.mem.eram_start = &gb_state->saved.mem.rom_start[rom_banks_size];
  gb_state->saved.mem.eram_size  = eram_banks_size;
}

// TODO: Create dispatch function for freeing whatever mbc is being used and
// make sure this is called when gb_state has been freed.
void gb_free_mbc1(gb_state_t *gb_state) {
  GB_free(gb_state->saved.mem.rom_start);
  gb_state->saved.mem.rom_start  = NULL;
  gb_state->saved.mem.eram_start = NULL;
}

void gb_alloc_mbc(gb_state_t *gb_state) {
  switch (gb_state->saved.header.mbc_type) {
  case GB_NO_MBC: gb_alloc_no_mbc(gb_state); break;
  case GB_MBC1: gb_alloc_mbc1(gb_state); break;
  case GB_MBC2:
  case GB_MBC3:
  case GB_MBC5:
  case GB_MBC7:
  case GB_MMM01:
  case GB_HUC1:
  case GB_HUC3:
  case GB_TPP1:
  case GB_CAMERA:
  case GB_MBC_UNKNOWN: NOT_IMPLEMENTED("Write attempted on MBC that is not yet implemented."); break;
  }
}
#define FSET_POS(f, pos)                                                                                               \
  {                                                                                                                    \
    err = fseek(f, pos, SEEK_SET);                                                                                     \
    GB_assert(err == 0);                                                                                               \
  }
#define FLEN(f, len)                                                                                                   \
  {                                                                                                                    \
    err = fseek(f, 0, SEEK_END);                                                                                       \
    GB_assert(err == 0);                                                                                               \
    len = ftell(f);                                                                                                    \
  }

bool gb_load_rom(struct gb_state *gb_state, const char *rom_name, const char *bootrom_name, const char *sym_name) {
  FILE *f;
  int   err;

  if (rom_name != NULL) {
    f = fopen(rom_name, "r");
    GB_assert(f != NULL);
    { // Read Cartridge Header
      FSET_POS(f, GB_HEADER_START);
      uint8_t header_bytes[GB_HEADER_SIZE];
      size_t  header_len;
      header_len = fread(header_bytes, sizeof(uint8_t), GB_HEADER_SIZE, f);
      GB_assert(header_len == GB_HEADER_SIZE);
      if ((err = ferror(f))) {
        LogCritical("Error when reading rom file: %d", err);
        return false;
      }
      gb_state->saved.header = gb_parse_cart_header(header_bytes);
    }
    { // Initialize MBC and Copy ROM
      gb_alloc_mbc(gb_state);
      size_t len;
      FLEN(f, len);
      FSET_POS(f, 0x00);
      size_t bytes_read = fread(gb_state->saved.mem.rom_start, 1, gb_state->saved.mem.rom_size, f);

      GB_assert(len == gb_state->saved.mem.rom_size);
      GB_assert(bytes_read == len);
    }
    fclose(f);
    gb_state->dbg.rom_loaded = true;
  }

  // Load debug symbols into gb_state->syms (symbols are optional)
  if (sym_name != NULL) {
    alloc_symbol_list(&gb_state->dbg.syms);
    f = fopen(sym_name, "r");
    parse_syms(&gb_state->dbg.syms, f);
    if ((err = ferror(f))) {
      LogCritical("Error when reading symbol file: %d", err);
      return false;
    }
    fclose(f);
  }
  gb_state_load_bootrom(gb_state, bootrom_name);

  return true;
}

#undef FSET_POS
#undef FLEN

void gb_state_load_bootrom(struct gb_state *gb_state, const char *bootrom_name) {
  // Load bootrom into gb_state->bootrom (bootrom is optional)
  if (bootrom_name != NULL) {
    FILE *f;
    int   bytes_len;
    int   err;
    f         = fopen(bootrom_name, "r");
    bytes_len = fread(gb_state->saved.mem.bootrom, sizeof(uint8_t), 0x0100, f);
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
  memcpy(gb_state->saved.mem.bootrom, dmg0_boot_rom_data, 0x0100);
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

static void *gb_unmap_mbc1_address(gb_state_t *gb_state, uint16_t addr) {
  if (addr <= ROM0_END) {

    uint8_t bank = 0;
    if (gb_state->saved.regs.mbc1_regs.banking_mode_select == MBC1_BANK_MODE_ADVANCED) {
      bank = gb_state->saved.regs.mbc1_regs.rom_bank_upper * 0x20;
      bank &= (gb_state->saved.header.num_rom_banks - 1);
    }
    return &gb_state->saved.mem.rom_start[(KB(16) * bank) + (addr - ROM0_START)];
  }
  if (addr <= ROMN_END) {
    uint8_t bank = gb_state->saved.regs.mbc1_regs.rom_bank;
    bank |= (((gb_state->saved.regs.mbc1_regs.rom_bank_upper & 0b11) << 5));
    bank &= (gb_state->saved.header.num_rom_banks - 1);
    return &gb_state->saved.mem.rom_start[(KB(16) * bank) + (addr - ROMN_START)];
  }
  if (addr >= ERAM_START && addr <= ERAM_END) {
    if (gb_state->saved.regs.mbc1_regs.ram_enable) {
      GB_assert(gb_state->saved.header.num_ram_banks <= 4);
      uint8_t bank = gb_state->saved.regs.mbc1_regs.ram_bank;
      bank &= (gb_state->saved.header.num_ram_banks - 1);

      return &gb_state->saved.mem.eram_start[(KB(8) * bank) + (addr - ERAM_START)];
    }
    LogDebug("MBC1 ERAM Read without ram_enabled set.");
    return NULL;
  }
  ERR(gb_state, "Invalid MBC1 address unmapped $%.4X.", addr);
  return NULL;
}
static void *gb_unmap_no_mbc_address(gb_state_t *gb_state, uint16_t addr) {
  if (addr <= ROM0_END) {
    return &gb_state->saved.mem.rom_start[(KB(16) * 0) + (addr - ROM0_START)];
  }
  if (addr <= ROMN_END) {
    return &gb_state->saved.mem.rom_start[(KB(16) * 1) + (addr - ROMN_START)];
  }
  if (addr >= ERAM_START && addr <= ERAM_END) {
    return &gb_state->saved.mem.eram_start[(KB(8) * 0) + (addr - ERAM_START)];
  }
  ERR(gb_state, "Invalid NO_MBC address unmapped $%.4X.", addr);
  return NULL;
}
static void *gb_unmap_mbc_address(gb_state_t *gb_state, uint16_t addr) {
  switch (gb_state->saved.header.mbc_type) {
  case GB_NO_MBC: return gb_unmap_no_mbc_address(gb_state, addr);
  case GB_MBC1: return gb_unmap_mbc1_address(gb_state, addr);
  case GB_MBC2:
  case GB_MBC3:
  case GB_MBC5:
  case GB_MBC7:
  case GB_MMM01:
  case GB_HUC1:
  case GB_HUC3:
  case GB_TPP1:
  case GB_CAMERA:
  case GB_MBC_UNKNOWN: NOT_IMPLEMENTED("Unmap attempted on MBC that is not yet implemented."); return NULL;
  default: unreachable();
  }
}

void *gb_unmap_address(gb_state_t *gb_state, uint16_t addr) {
  if (gb_state->saved.regs.io.bank && (addr < 0x0100)) {
    return &gb_state->saved.mem.bootrom[addr];
  }
  if (addr <= ROM0_END) {
    return gb_unmap_mbc_address(gb_state, addr);
  } else if (addr <= ROMN_END) {
    return gb_unmap_mbc_address(gb_state, addr);
  } else if (addr <= VRAM_END) {
    return &gb_state->saved.mem.vram[addr - VRAM_START];
  } else if (addr <= ERAM_END) {
    // TODO: implement eram bank switching
    return gb_unmap_mbc_address(gb_state, addr);
  } else if (addr <= WRAM_END) {
    return &gb_state->saved.mem.wram[addr - WRAM_START];
  } else if (addr <= ECHO_RAM_END) {
    // Mirrors wram, probably should never be accessed.
    return &gb_state->saved.mem.wram[addr - ECHO_RAM_START];
  } else if (addr <= OAM_END) {
    return &gb_state->saved.mem.oam[addr - OAM_START];
  } else if (addr <= IO_REG_END) {
    goto not_implemented;
  } else if (addr <= HRAM_END) {
    return &gb_state->saved.mem.hram[addr - HRAM_START];
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

static void gb_write_mbc1(gb_state_t *gb_state, uint16_t addr, uint8_t val) {
  GB_assert(addr < 0x8000);
  // There are 4 unique places in memory that mbc1 receives writes to. Which of these is written to is determined by
  // bits 14 and 13.
  uint8_t      bank_reg  = addr >> 13;
  mbc1_regs_t &mbc1_regs = gb_state->saved.regs.mbc1_regs;
  switch (bank_reg) {
  case 0: // 0x0000-0x1FFF
    // It seems like it's unknown why they just check if the lower 4 bits are 0xA.
    mbc1_regs.ram_enable = ((val & 0x0F) == 0x0A);
    break;
  case 1: // 0x2000-0x3FFF
    mbc1_regs.rom_bank = (val & 0b0001'1111);
    // 0 reads as if it is 1 to prevent mapping bank 0 to both areas. This needs to happen before the not needed bits
    // are masked out.
    if (mbc1_regs.rom_bank == 0) mbc1_regs.rom_bank = 1;

    if (mbc1_regs.rom_bank >= gb_state->saved.header.num_rom_banks) {
      mbc1_regs.rom_bank &= gb_state->saved.header.num_rom_banks - 1;
    }
    break;
  case 2: // 0x4000-0x5FFF
    mbc1_regs.rom_bank_upper = (val & 0b11);
    // These should be the same value since they are in an anonymous union. I just gave them two names for clarity.
    assert(mbc1_regs.rom_bank_upper == mbc1_regs.ram_bank);
    break;
  case 3: // 0x6000-0x7FFF
    mbc1_regs.banking_mode_select = (mbc1_bank_mode_t)(val & 0b1);
    break;
  default: unreachable(); break;
  }
}

// Called whenever gb_write_mem is called on ROM.
static void gb_write_mbc(gb_state_t *gb_state, uint16_t addr, uint8_t val) {
  switch (gb_state->saved.header.mbc_type) {
  case GB_NO_MBC: break;
  case GB_MBC1: gb_write_mbc1(gb_state, addr, val); break;
  case GB_MBC2:
  case GB_MBC3:
  case GB_MBC5:
  case GB_MBC7:
  case GB_MMM01:
  case GB_HUC1:
  case GB_HUC3:
  case GB_TPP1:
  case GB_CAMERA:
  case GB_MBC_UNKNOWN: NOT_IMPLEMENTED("Write attempted on MBC that is not yet implemented."); break;
  }
}

void gb_write_mem(struct gb_state *gb_state, uint16_t addr, uint8_t val) {
  if (gb_state->dbg.use_flat_ram) {
    gb_state->saved.flat_ram[addr] = val;
    return;
  }
  if (addr < 0x8000) {
    gb_write_mbc(gb_state, addr, val);
    return;
  }

  LogTrace("Writing val 0x%.2X to address 0x%.4X", val, addr);
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
  }

  uint8_t *val_ptr = ((uint8_t *)gb_unmap_address(gb_state, addr));
  if (val_ptr == NULL) {
    LogCritical("`write_mem()` received a null pointer from `gb_unmap_address()` when addr = 0x%04x", addr);
    return;
  }
  // VRAM is the only place where stuff needs to be uploaded to the GPU (which is expensive). So we mark modified
  // items in vram as dirty where necessary.
  if (addr >= VRAM_START && addr <= VRAM_END) mark_dirty(gb_state, addr);
  *val_ptr = val;
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
