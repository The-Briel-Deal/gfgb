#include "apu.h"
#include "common.h"

gb_apu_t::gb_apu(gb_state_t &gb_state) : parent(gb_state) { CheckedSDL(Init(SDL_INIT_AUDIO)); }
void gb_apu_t::update() {
  // TODO: Go through audio registers and play sound accordingly.
}
