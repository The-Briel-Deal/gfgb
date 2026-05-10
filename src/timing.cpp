#include "common.h"
#include "ppu.h"
#include "tracy/Tracy.hpp"

#define PPU_DOTS_PER_FRAME  70224
#define PPU_DOTS_PER_LINE   456
#define PPU_LINES_PER_FRAME 154

#define VBLANK_Y_START 144

#define TAC_ENABLE_BIT 0b0000'0100

static void gb_incr_tima(gb_state_t *gb_state) {
  if (gb_state->saved.regs.io.tima == 0xFF) {
    gb_state->saved.regs.io.if_ |= 0b00100;
    // reset TIMA to TMA since TIMA is going to overflow
    // https://gbdev.io/pandocs/Timer_and_Divider_Registers.html#ff06--tma-timer-modulo
    gb_state->saved.regs.io.tima = gb_state->saved.regs.io.tma;
  } else {
    gb_state->saved.regs.io.tima++;
  }
}

void gb_handle_div_write(gb_state_t *gb_state) {
  gb_state->timing.sysclk     = 0;
  gb_state->saved.regs.io.div = 0;
  if (falling_edge(gb_state->saved.last_tima_bit, 0)) {
    gb_incr_tima(gb_state);
  }
  gb_state->saved.last_tima_bit = 0;
}

static void gb_sync_tima(gb_state_t *gb_state, uint16_t old_sysclk, uint16_t new_sysclk) {
  uint8_t tac = gb_state->saved.regs.io.tac;
  // TODO: This is slow but accurate, since we increment multiple cycles at once in many places we need to make sure
  // that we don't miss the falling edge. There's probably a better way to do this if I take a bit to think on this.
  for (uint16_t curr_sysclk = old_sysclk; curr_sysclk != new_sysclk; curr_sysclk++) {
    bool this_bit = 0;
    switch (tac & 0b0000'0011) {
      case 0: this_bit = (curr_sysclk >> 9) & 1; break;
      case 3: this_bit = (curr_sysclk >> 7) & 1; break;
      case 2: this_bit = (curr_sysclk >> 5) & 1; break;
      case 1: this_bit = (curr_sysclk >> 3) & 1; break;
    }
    // Don't increment TIMA if the enable bit is clear.
    this_bit &= ((tac & TAC_ENABLE_BIT) >> 2);

    // Only increment on falling edge (a.k.a. true -> false).
    if (falling_edge(gb_state->saved.last_tima_bit, this_bit)) {
      gb_incr_tima(gb_state);
    }

    gb_state->saved.last_tima_bit = this_bit;
  }
}
static void gb_sync_lcd_stat(gb_state_t *gb_state) {
  uint8_t &stat      = gb_state->saved.regs.io.stat;
  uint8_t  lyc_eq_ly = (gb_state->saved.regs.io.ly == gb_state->saved.regs.io.lyc);
  stat &= 0b1111'1011;
  stat |= (lyc_eq_ly << 2);

  uint8_t mode       = (stat & (0b11 << 0)) >> 0;
  uint8_t m0_select  = (stat & (0b1 << 3)) >> 3;
  uint8_t m1_select  = (stat & (0b1 << 4)) >> 4;
  uint8_t m2_select  = (stat & (0b1 << 5)) >> 5;
  uint8_t lyc_select = (stat & (0b1 << 6)) >> 6;

  bool stat_interrupt = false;
  switch (mode) {
    case HBLANK:
      if (m0_select) stat_interrupt |= true;
      break;
    case VBLANK:
      if (m1_select) stat_interrupt |= true;
      if (m2_select && gb_state->saved.regs.io.ly == 144) stat_interrupt |= true;
      break;
    case OAM_SCAN:
      if (m2_select) stat_interrupt |= true;
      break;
    case DRAWING_PIXELS: break;
  }

  if (lyc_select && lyc_eq_ly) stat_interrupt |= true;

  if (rising_edge(gb_state->saved.last_stat_bit, stat_interrupt)) {
    gb_state->saved.regs.io.if_ |= 0b00010;
  }
  gb_state->saved.last_stat_bit = stat_interrupt;
}

static void gb_ppu_mode_change(gb_state_t *gb_state, gb_ppu_mode_t new_mode) {
  gb_ppu_mode_t old_mode = (gb_ppu_mode_t)(gb_state->saved.regs.io.stat & 0b11);
  if (old_mode != new_mode) {
    gb_state->saved.regs.io.stat &= 0b1111'1100;
    gb_state->saved.regs.io.stat |= new_mode;

    if (new_mode == VBLANK) {
      // The V-Blank interrupt handler seems to have the simplest rules out of all interrupts and I think just always
      // setting this when the mode changes to v-blank should be sufficient unless i'm missing something.
      gb_state->saved.regs.io.if_ |= 0b00001;
    }

    if (!gb_state->dbg.headless_mode) {
      ZoneScopedN("Rendering");
      switch (new_mode) {
        case OAM_SCAN: {
          ZoneScopedN("OAM Read");
          gb_read_oam_entries(gb_state);
          break;
        }
        case DRAWING_PIXELS: {
          ZoneScopedN("Drawing Pixels");
          gb_draw(gb_state);
          break;
        }
        case HBLANK: {
          ZoneScopedN("H-Blank");
          gb_composite_line(gb_state);
          if (gb_state->dbg.pause_next_hblank) {
            gb_state->dbg.next_line_hit();
          }
          break;
        }
        case VBLANK: {
          ZoneScopedN("V-Blank");
          gb_flip_frame(gb_state);
          if (gb_state->dbg.pause_next_vblank) {
            gb_state->dbg.next_frame_hit();
          }
          break;
        }
      }
    }
  }
}
static void gb_ppu_handle_oam_dma(gb_state_t *gb_state) {
  GB_assert(gb_state->video.oam_dma_start);
  gb_ppu_mode_t curr_mode = (gb_ppu_mode_t)(gb_state->saved.regs.io.stat & 0b11);
  // TODO: If I want perfect accuracy then I should be copying this incrementally on every iteration for 160
  // m-cycles. I also need to make all memory except hram blocked during this period.

  // TODO: There are some quirks when performing a dma transfer mid line (during OAM_SCAN or DRAWING_PIXELS), i'm
  // currently not sure if this will matter with any real world games so I should look into this.
  if (curr_mode == OAM_SCAN || curr_mode == DRAWING_PIXELS || curr_mode == VBLANK) {
    gb_state->video.oam_dma_start = false;
    uint8_t oam_dma               = gb_state->saved.regs.io.dma;
    if (oam_dma > 0xDF) {
      oam_dma -= 0x20;
    }
    uint16_t start_src_addr = ((uint16_t)oam_dma) << 8;
    for (uint8_t addr_offset = 0; addr_offset <= 0x9F; addr_offset++) {
      uint16_t src_addr = start_src_addr | addr_offset;
      uint16_t dst_addr = 0xFE00 | addr_offset;
      uint8_t  src_byte = gb_read_mem(gb_state, src_addr);
      gb_write_mem(gb_state, dst_addr, src_byte);
    }
  }
}

static void gb_ppu_spend_dots(gb_state_t *gb_state, uint16_t n) {
  uint32_t &dots = gb_state->timing.ppu_frame_dots;
  dots += n;
  if (dots >= PPU_DOTS_PER_FRAME) {
    dots %= PPU_DOTS_PER_FRAME;
    gb_state->video.frame_num += 1;
  }
  uint16_t line_x = (dots % PPU_DOTS_PER_LINE);
  uint16_t line_y = (dots / PPU_DOTS_PER_LINE);
  GB_assert(line_y < PPU_LINES_PER_FRAME);
  gb_state->saved.regs.io.ly = line_y;

  if (line_y >= VBLANK_Y_START) {
    gb_ppu_mode_change(gb_state, VBLANK);
  } else if (line_x < 80) {
    gb_ppu_mode_change(gb_state, OAM_SCAN);
  } else if (line_x < 80 + 289) {                 // Assuming longest possible drawing pixels duration, I would need
    gb_ppu_mode_change(gb_state, DRAWING_PIXELS); // to factor in drawing penalties to get an accurate length.
  } else {
    gb_ppu_mode_change(gb_state, HBLANK);
  }
  gb_sync_lcd_stat(gb_state);
  if (gb_state->video.oam_dma_start) gb_ppu_handle_oam_dma(gb_state);
}

void gb_spend_mcycles(gb_state_t *gb_state, uint16_t mcycles) {
  uint16_t tcycles = mcycles * 4;
  uint16_t dots    = mcycles * 4; // TODO: This will be different from tcycles once I impl GBC double speed mode.
  gb_state->saved.m_cycles_elapsed += mcycles;
  uint16_t old_sysclk     = gb_state->timing.sysclk;
  uint16_t new_sysclk     = gb_state->timing.sysclk + tcycles;
  gb_state->timing.sysclk = new_sysclk;
  gb_sync_tima(gb_state, old_sysclk, new_sysclk);
  uint8_t old_div             = gb_state->saved.regs.io.div;
  uint8_t new_div             = (gb_state->timing.sysclk & 0xFF00) >> 8;
  gb_state->saved.regs.io.div = new_div;
  if (falling_edge_bit(4, old_div, new_div)) {
    gb_state->apu.div_tick();
  }

  gb_ppu_spend_dots(gb_state, dots);
  gb_state->apu.spend_mcycles(mcycles); // TODO: This will be different from tcycles once I impl GBC double speed mode.
}
