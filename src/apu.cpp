#include "apu.h"
#include "common.h"

#define MAX_PERIOD           ((1 << 11) - 1)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8

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

static float gb_period_to_tone_freq(uint16_t period) {
  // Period can only be up to 11 bits (aka having a decimal max of 2047)
  GB_assert(period <= MAX_PERIOD);
  // Formula from https://gbdev.io/pandocs/Audio_Registers.html#ff13--nr13-channel-1-period-low-write-only
  float tone_freq = ((float)APU_CLOCK / SAMPLES_PER_WAVEFORM) / (2048 - period);
  return tone_freq;
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
      // TODO: Impl Channel 1
      // TODO: Sweep functionality (NR10)
      // TODO: Length Timer and Duty Cycle (NR11)

      uint16_t period = io_regs.nr13;
      period |= io_regs.nr14;
      period &= MAX_PERIOD;
      uint16_t tone_freq = gb_period_to_tone_freq(period);
    }
    if (ch2_on) {
      // TODO: Impl Channel 2
    }
    if (ch3_on) {
      // TODO: Impl Channel 3
    }
    if (ch4_on) {
      // TODO: Impl Channel 4
    }
  }
}
