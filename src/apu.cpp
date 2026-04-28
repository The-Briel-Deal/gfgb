#include "apu.h"
#include "common.h"

#define MAX_PERIOD           ((1 << 11) - 1)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8
#define CH_IS_ON(chan_nr)    ((io_regs.nr52 >> (chan_nr - 1)) & 1)

gb_pulsewave_channel_t::gb_pulsewave_channel() : phase(0), counter(2048), period(0) {}

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
void gb_apu_t::sync_regs() {
  io_regs_t &io_regs       = this->parent.saved.regs.io;
  bool       ch1_triggered = (io_regs.nr14 >> 7) & 1;
  bool       ch2_triggered = (io_regs.nr24 >> 7) & 1;
  bool       ch3_triggered = (io_regs.nr34 >> 7) & 1;
  bool       ch4_triggered = (io_regs.nr44 >> 7) & 1;
  if (ch1_triggered) {
    io_regs.nr52 |= (1 << 0);
    io_regs.nr14 &= ~(1 << 7);
  }
  if (ch2_triggered) {
    io_regs.nr52 |= (1 << 1);
    io_regs.nr24 &= ~(1 << 7);
  }
  if (ch3_triggered) {
    io_regs.nr52 |= (1 << 2);
    io_regs.nr34 &= ~(1 << 7);
  }
  if (ch4_triggered) {
    io_regs.nr52 |= (1 << 3);
    io_regs.nr44 &= ~(1 << 7);
  }

  this->ch1.period = io_regs.nr13 | ((io_regs.nr14 & 0b0000'0111) << 8);
}

void gb_apu_t::spend_mcycles(uint16_t m_cycles) {
  for (uint16_t i = 0; i < m_cycles; i++)
    this->tick();
}

void gb_apu_t::tick() {
  io_regs_t &io_regs = this->parent.saved.regs.io;
  // TODO: This doesn't need to be called every tick, I could also just do this on write for each NRx4.
  this->sync_regs();

  // TODO: Go through the rest of the audio registers and play sound accordingly.
  bool apu_powered_on = ((io_regs.nr52 >> 7) & 1);
  if (apu_powered_on) {
    if (CH_IS_ON(1)) {
      gb_pulsewave_channel_t &ch = this->ch1;
      // TODO: Sweep functionality (NR10)
      // TODO: Length Timer and Duty Cycle (NR11)
      ch.counter--;
      if (ch.counter == 0) {
        ch.counter = 2048 - ch.period;
        ch.phase++;
        ch.phase &= 0b0000'0111; // Equal to `ch.phase %= 8` except without the expensive mod.
      }
    }
    if (CH_IS_ON(2)) {
      // TODO: Impl Channel 2
    }
    if (CH_IS_ON(3)) {
      // TODO: Impl Channel 3
    }
    if (CH_IS_ON(4)) {
      // TODO: Impl Channel 4
    }
  }
}
