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
