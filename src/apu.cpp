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
  io_regs_t &io_regs       = this->parent.saved.regs.io;
  bool       ch1_triggered = (io_regs.nr14 >> 7) & 1;
  if (ch1_triggered) {
    io_regs.nr52 |= (1 << 0);
    io_regs.nr14 &= ~(1 << 7);
  }

  // TODO: Go through the rest of the audio registers and play sound accordingly.
  bool apu_powered_on = ((io_regs.nr52 >> 7) & 1);
  if (apu_powered_on) {
    // TODO: These will need to actually be set by me somewhere since these are read only.
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

      const int minimum_audio = (8000 * sizeof(float)) / 2; /* 8000 float samples per second. Half of that. */
      if (SDL_GetAudioStreamQueued(this->output_stream) < minimum_audio) {
        static float samples[2048]; /* this will feed 512 samples each frame until we get to our maximum. */
        size_t       i;

        /* generate a 440Hz pure tone */
        for (i = 0; i < SDL_arraysize(samples); i++) {
          const float phase  = this->current_sine_sample * tone_freq / 8000.0f;
          float       sample = SDL_sinf(phase * 2 * SDL_PI_F);
          if (sample >= 0)
            samples[i] = 1;
          else
            samples[i] = -1;
          this->current_sine_sample++;
        }

        /* wrapping around to avoid floating-point errors */
        this->current_sine_sample %= 8000;

        /* feed the new data to the stream. It will queue at the end, and trickle out as the hardware needs more data.
         */
        SDL_PutAudioStreamData(this->output_stream, samples, sizeof(samples));

        SDL_ResumeAudioStreamDevice(this->output_stream);
      }
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
