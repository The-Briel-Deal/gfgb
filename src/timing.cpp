#include "common.h"
#include "ppu.h"
#include "tracy/Tracy.hpp"

#define PPU_DOTS_PER_FRAME  70224
#define PPU_DOTS_PER_LINE   456
#define PPU_LINES_PER_FRAME 154

#define VBLANK_Y_START 144

static void gb_ppu_mode_change(gb_state_t *gb_state, gb_ppu_mode_t new_mode) {
  gb_ppu_mode_t old_mode = gb_state->video.ppu_mode;
  if (old_mode != new_mode) {
    gb_state->video.ppu_mode = new_mode;

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
        break;
      }
      case VBLANK: {
        ZoneScopedN("V-Blank");
        gb_flip_frame(gb_state);
        break;
      }
      }
    }
  }
}
static void gb_ppu_handle_oam_dma(gb_state_t *gb_state) {
  GB_assert(gb_state->video.oam_dma_start);
  gb_ppu_mode_t curr_mode = gb_state->video.ppu_mode;
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
      uint8_t  src_byte = gb_read_mem8(gb_state, src_addr);
      gb_write_mem8(gb_state, dst_addr, src_byte);
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
  if (gb_state->video.oam_dma_start) gb_ppu_handle_oam_dma(gb_state);
}

void gb_spend_mcycles(gb_state_t *gb_state, uint16_t mcycles) {
  uint16_t tcycles = mcycles * 4;
  uint16_t dots    = mcycles * 4; // This will be different from tcycles once I impl GBC double speed mode.
  gb_state->saved.m_cycles_elapsed += mcycles;
  gb_state->timing.sysclk += tcycles;
  gb_state->saved.regs.io.div = (gb_state->timing.sysclk & 0xFF00) >> 8;
  gb_ppu_spend_dots(gb_state, dots);
}
