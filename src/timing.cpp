#include "common.h"
#include "ppu.h"

#define PPU_DOTS_PER_FRAME 70224
#define PPU_DOTS_PER_LINE  456

static void gb_ppu_spend_dots(gb_state_t *gb_state, uint16_t n) {
  uint32_t &dots = gb_state->timing.ppu_frame_dots;
  gb_state->timing.ppu_frame_dots += n;
  if (gb_state->timing.ppu_frame_dots >= PPU_DOTS_PER_FRAME) {
    gb_state->timing.ppu_frame_dots %= PPU_DOTS_PER_FRAME;
    gb_state->video.frame_num += 1;
  }
  if (gb_state->timing.ppu_frame_dots >= (PPU_DOTS_PER_LINE * 144)) {
    gb_state->video.ppu_mode = VBLANK;
  }
  if () {
  }

  ERR(gb_state, "Mode not found at ppu_frame_dots=%d", );
mode_found:
}

void gb_spend_mcycles(gb_state_t *gb_state, uint16_t n) {
  gb_state->saved.m_cycles_elapsed += n;
  gb_state->timing.sysclk += (n * 4);
  gb_state->saved.regs.io.div = (gb_state->timing.sysclk & 0xFF00) >> 8;
}
