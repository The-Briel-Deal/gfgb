#include "apu.h"
#include "common.h"
#include <SDL3/SDL_audio.h>

gb_apu_t::gb_apu(gb_state_t &gb_state) : parent(gb_state) {
  CheckedSDL(Init(SDL_INIT_AUDIO));

  SDL_AudioSpec spec = {
      .format   = SDL_AUDIO_F32,
      .channels = 1,
      .freq     = 8000,
  };
  this->output_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
  if (!this->output_stream) {
    Err((&gb_state), "Couldn't create audio stream: %s", SDL_GetError());
  }
}
void gb_apu_t::update() {
  io_regs_t &io_regs = this->parent.saved.regs.io;
  // TODO: Go through the rest of the audio registers and play sound accordingly.
  bool apu_powered_on = ((io_regs.nr52 >> 7) & 1);
  if (apu_powered_on) {
    bool ch1_on = ((io_regs.nr52 >> 0) & 1);
    bool ch2_on = ((io_regs.nr52 >> 1) & 1);
    bool ch3_on = ((io_regs.nr52 >> 2) & 1);
    bool ch4_on = ((io_regs.nr52 >> 3) & 1);
    if (ch1_on) {
      // TODO: Impl
    }
    if (ch2_on) {
      // TODO: Impl
    }
    if (ch3_on) {
      // TODO: Impl
    }
    if (ch4_on) {
      // TODO: Impl
    }
  }
}
