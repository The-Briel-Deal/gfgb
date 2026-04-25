#include "common.h"
#include "ppu.h"
#include "timing.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <istream>
#include <regex>

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
  if (parsed_header.has_rtc) NOT_IMPLEMENTED("RTC (Real Time Clock) not implemented.");
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

// TODO: This doesn't seem to always work, I need to figure out what other state I need set.
void gb_state_reset(struct gb_state *gb_state) {
  gb_state->saved.regs.sp                     = WRAM_END + 1;
  gb_state->saved.regs.pc                     = 0;
  gb_state->saved.regs.io.bank                = true;
  gb_state->video.first_oam_scan_after_enable = true;
  gb_state->saved.m_cycles_elapsed            = 0;
  gb_state->saved.last_timer_sync_m_cycles    = 0;
  gb_state->timing.ns_elapsed_while_running   = 0;
}

struct gb_state *gb_state_alloc() { return new gb_state_t; }

void gb_state_free(struct gb_state *gb_state) { delete gb_state; }

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
  case IO_NR52: return &gb_state->saved.regs.io.nr52;
  case IO_IF: return &gb_state->saved.regs.io.if_;
  case IO_IE: return &gb_state->saved.regs.io.ie;
  case IO_DMA: return &gb_state->saved.regs.io.dma;
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
  default:
    // Since I don't have all IO regs implemented yet, this log was getting really noisy in the error severity. Maybe
    // once I have all IO regs impl'd I can move this back to the error sev.
    LogDebugCat(GB_LOG_CATEGORY_IO_REGS, "IO Reg Not Implemented at addr 0x%04X", addr);
    ErrQuiet(gb_state);
    return NULL;
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
    LogDebug("`read_mem()` received a null pointer from `gb_unmap_address()` when addr = 0x%.4X", addr);
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
  io_regs_t &io_regs = gb_state->saved.regs.io;
  // Some IO registers require special handling, like the joypad reg where bit 5 and 4 are read/write, while 3-0 are
  // read-only.
  LogDebugCat(GB_LOG_CATEGORY_IO_REGS, "Writing val = 0x%.2X to IO Reg at addr = 0x%.4X", val, reg);
  switch (reg) {
  case IO_SC:
    // TODO: CGB uses bit 1 for clock speed. I'll need to implement that if I add CGB support.
    *get_io_reg(gb_state, IO_SC) = val | 0b0111'1110;
    break;
  case IO_JOYP:
    io_regs.joyp &= ~(JOYP_SELECT_BUTTONS | JOYP_SELECT_D_PAD);
    io_regs.joyp |= (val & (JOYP_SELECT_BUTTONS | JOYP_SELECT_D_PAD));
    break;
  case IO_DIV: gb_handle_div_write(gb_state); break;
  case IO_BANK:
    // if bit 0 is set unmap bootrom. This can't be re-enabled without a restart.
    if (val & 1) {
      io_regs.bank = false;
    }
    break;

  case IO_NR52:
    // Audio on/off (bit 7) is the only writable bit
    io_regs.nr52 &= 0b0111'1111;
    io_regs.nr52 |= (val & 0b1000'0000);
    break;
  default:
    if (reg == IO_DMA) {
      gb_state->video.oam_dma_start = true;
    }
    uint8_t *reg_ptr = get_io_reg(gb_state, reg);
    if (reg_ptr == NULL) {
      // Since I don't have all IO regs implemented yet, this log was getting really noisy in the error severity. Maybe
      // once I have all IO regs impl'd I can move this back to the error sev.
      LogDebugCat(GB_LOG_CATEGORY_IO_REGS, "IO Reg Not Implemented at addr 0x%04X", reg);
      ErrQuiet(gb_state);
      break;
    }
    *reg_ptr = val;
    break;
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
    // This isn't normally concerning, unmapping ERAM when the eram register is disabled will do this.
    LogDebug("`write_mem()` received a null pointer from `gb_unmap_address()` when addr = 0x%04x", addr);
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

gb_state::gb_state() : apu(*this) {
  /// Registers
  // It looks like this was originally at the top of HRAM, but some emulators
  // set SP to the top of WRAM, since I don't have HRAM implemented yet I'm
  // going with the latter approach for now.
  this->saved.regs         = {};
  this->saved.regs.sp      = WRAM_END + 1;
  this->saved.regs.io.lcdc = 0b1001'0001;
  this->saved.regs.io.sc   = 0b0111'1110;

  this->saved.mem            = {};
  this->saved.mem.mbc_mem    = NULL;
  this->saved.mem.rom_start  = NULL;
  this->saved.mem.eram_start = NULL;

  this->saved.halted                   = false;
  this->saved.last_tima_bit            = false;
  this->saved.last_stat_bit            = false;
  this->saved.wy_cond                  = false;
  this->saved.wx_cond                  = false;
  this->saved.win_line_blank           = false;
  this->saved.m_cycles_elapsed         = 0;
  this->saved.last_timer_sync_m_cycles = 0;
  this->saved.win_line_counter         = 0;
  this->saved.oam_entries[0]           = NULL; // This is a null terminated list of pointers.

  this->timing = {};

  /// Video State
  this->video                             = {};
  this->video.initialized                 = false;
  this->video.oam_dma_start               = false;
  this->video.first_oam_scan_after_enable = true;

  /// Debug State
  this->dbg                         = {};
  this->dbg.step_inst_count         = 0;
  this->dbg.speed_factor            = 1.0;
  this->dbg.test_mode               = false;
  this->dbg.execution_paused        = false;
  this->dbg.err                     = false;
  this->dbg.pause_on_err            = false;
  this->dbg.use_flat_ram            = false;
  this->dbg.headless_mode           = false;
  this->dbg.trace_exec              = false;
  this->dbg.trace_exec_fout         = NULL;
  this->dbg.serial_port_output_file = NULL;
  this->dbg.syms                    = {.syms = NULL, .len = 0, .capacity = 0};

  this->breakpoints               = new std::vector<gb_breakpoint_t>;
  this->serial_port_output_string = new std::string;
  this->compiled_pass_regex       = new std::basic_regex<char>;
  this->compiled_fail_regex       = new std::basic_regex<char>;
}

bool gb_state::load_rom(const str rom_filename) {
  uint8_t       header_bytes[GB_HEADER_SIZE];
  std::ifstream f(rom_filename);

  // Read Cartridge Header
  f.seekg(GB_HEADER_START, std::ifstream::beg);
  f.read((char *)header_bytes, GB_HEADER_SIZE);
  if (!f.good()) {
    LogCritical("Error when reading rom file: %s", strerror(errno));
    return false;
  }
  this->saved.header = gb_parse_cart_header(header_bytes);

  // Initialize MBC and Copy ROM
  gb_alloc_mbc(this);

  f.seekg(0, std::ifstream::beg);
  f.read((char *)this->saved.mem.rom_start, this->saved.mem.rom_size);

  f.close();
  this->dbg.rom_loaded = true;
  return true;
}

bool gb_state::load_bootrom(const str bootrom_filename) {

  std::ifstream f(bootrom_filename);
  f.read((char *)this->saved.mem.bootrom, 0x0100);
  if (!f.good()) {
    LogError("Error when reading bootrom file: %s", strerror(errno));
    return false;
  }
  f.close();
  this->saved.regs.pc      = 0x0000;
  this->saved.regs.io.bank = true;
  return true;
}

bool gb_state::load_bootrom() {
  memcpy(this->saved.mem.bootrom, dmg0_boot_rom_data, 0x0100);
  this->saved.regs.pc      = 0x0000;
  this->saved.regs.io.bank = true;
  return true;
}

bool gb_state::load_syms(std::istream &sym_stream) {
  if (this->dbg.syms.syms == NULL) {
    alloc_symbol_list(&this->dbg.syms);
  }

  str                  line;
  debug_symbol_list_t *syms = &this->dbg.syms;
  while (std::getline(sym_stream, line)) {
    realloc_symbol_list(syms);
    struct debug_symbol &curr_sym = syms->syms[syms->len];

    // Check if this is a comment line.
    static const std::regex comment_line_pattern("^\\s*;.*");
    if (std::regex_match(line, comment_line_pattern)) {
      continue;
    }

    static const std::regex symbol_line_pattern(
        "([0-9|A-F|a-f]{0,8}|BOOT):([0-9|A-F|a-f]{0,4}) ([A-Za-z_]{1,1}[A-Za-z0-9_@#$.]*)$");
    std::smatch matches;
    if (std::regex_match(line, matches, symbol_line_pattern)) {
      if (matches[1] == "BOOT") {
        curr_sym.bank = -1;
      } else {
        curr_sym.bank = std::stoi(matches[1], 0, 16);
      }
      curr_sym.start_offset   = std::stoi(matches[2], 0, 16);
      str      sym_name       = matches[3];
      uint32_t n_copied       = sym_name.copy(curr_sym.name, sizeof(curr_sym.name) - 1);
      curr_sym.name[n_copied] = '\0';
      syms->len++;
      GB_assert(syms->len < syms->capacity);
      continue;
    }
    LogError("Line did not match expected pattern: '%s'", line.c_str());
  }
  if (sym_stream.fail() && !sym_stream.eof()) {
    LogCritical("Error when reading sym file: %s", strerror(errno));
    return false;
  }
  sort_syms(syms);
  set_sym_lens(syms);
  return true;
}

bool gb_state::load_syms(const str sym_filename) {
  std::ifstream f(sym_filename);
  return this->load_syms(f);
}

void gb_state::init_no_bootrom() {
  // I set a breakpoint at $0100 in DMG-0 bootrom and just dumped all registers state in GDB. These were the values I
  // got. They seem to work but it's possible i'm missing something.
  this->saved.regs.a  = 1;
  this->saved.regs.b  = 255;
  this->saved.regs.c  = 19;
  this->saved.regs.d  = 0;
  this->saved.regs.e  = 193;
  this->saved.regs.f  = 0;
  this->saved.regs.h  = 132;
  this->saved.regs.l  = 3;
  this->saved.regs.sp = 65534;
  this->saved.regs.pc = 0x0100;
  this->saved.regs.io = {.joyp = 207,
                         .sb   = 0,
                         .sc   = 126,
                         .div  = 47,
                         .tima = 0,
                         .tma  = 0,
                         .tac  = 0,

                         .nr10 = 0,
                         .nr11 = 0,
                         .nr12 = 0,
                         .nr13 = 0,
                         .nr14 = 0,

                         .nr21 = 0,
                         .nr22 = 0,
                         .nr23 = 0,
                         .nr24 = 0,

                         .nr30 = 0,
                         .nr31 = 0,
                         .nr32 = 0,
                         .nr33 = 0,
                         .nr34 = 0,

                         .nr41 = 0,
                         .nr42 = 0,
                         .nr43 = 0,
                         .nr44 = 0,

                         .nr50          = 0,
                         .nr51          = 0,
                         .nr52          = 0, // sound on/off
                         .ly            = 144,
                         .lyc           = 0,
                         .stat          = 1,
                         .lcdc          = 145,
                         .scy           = 0,
                         .scx           = 0,
                         .bgp           = 252,
                         .obp0          = 0,
                         .obp1          = 0,
                         .wx            = 0,
                         .wy            = 0,
                         .ie            = 0,
                         .if_           = 1,
                         .dma           = 0,
                         .ime           = false,
                         .set_ime_after = false,
                         .bank          = false};
}
bool gb_state::load_rom(const str rom_filename, const opt<str> bootrom_filename, const opt<str> sym_filename,
                        gb_load_rom_opts_t opts) {
  bool success = true;
  success &= this->load_rom(rom_filename);
  if (sym_filename) {
    success &= this->load_syms(*sym_filename);
  }
  if (opts & GB_LOAD_ROM_NO_BOOTROM) {
    this->init_no_bootrom();
    goto bootrom_loaded;
  }
  if (bootrom_filename) {
    success &= this->load_bootrom(*bootrom_filename);
    goto bootrom_loaded;
  }
  success &= this->load_bootrom();

bootrom_loaded:

  return success;
}

void gb_dbg_state::pause() {
  GB_assert(!this->execution_paused);
  this->execution_paused = true;
}

void gb_dbg_state::cont() {
  GB_assert(this->execution_paused);
  this->execution_paused = false;
}

void gb_dbg_state::next_frame() {
  GB_assert(this->execution_paused);
  this->cont();
  this->pause_next_vblank = true;
}

void gb_dbg_state::next_frame_hit() {
  GB_assert(!this->execution_paused);
  GB_assert(this->pause_next_vblank);
  this->pause();
  this->pause_next_vblank = false;
}

void gb_dbg_state::next_line() {
  GB_assert(this->execution_paused);
  this->cont();
  this->pause_next_hblank = true;
}

void gb_dbg_state::next_line_hit() {
  GB_assert(!this->execution_paused);
  GB_assert(this->pause_next_hblank);
  this->pause();
  this->pause_next_hblank = false;
}

void gb_dbg_state::step_inst() {
  GB_assert(this->execution_paused);
  this->step_inst_count++;
}

gb_state::~gb_state() {

  if (this->dbg.serial_port_output_file != NULL) fclose(this->dbg.serial_port_output_file);

  if (this->dbg.syms.capacity > 0) {
    free_symbol_list(&this->dbg.syms);
  }
  delete this->breakpoints;
  delete this->serial_port_output_string;
  delete this->compiled_pass_regex;
  delete this->compiled_fail_regex;
  if (!this->dbg.use_flat_ram) {
    gb_free_mbc(this);
  }
}
