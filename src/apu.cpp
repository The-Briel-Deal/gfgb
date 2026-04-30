#include "apu.h"
#include "common.h"

#define MAX_PERIOD           (1 << 11)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8

#define AUDIO_SAMPLE_FREQ 8000
#define MIN_AUDIO_QUEUED                                                                                               \
  ((AUDIO_SAMPLE_FREQ * sizeof(float)) /                                                                               \
   60) /* We should have roughly a 60th of a second of audio queued at any given time  */

#define CH_IS_ON(chan_nr) ((io_regs.nr52 >> (chan_nr - 1)) & 1)

gb_pulsewave_channel_t::gb_pulsewave_channel() : phase(0), counter(MAX_PERIOD), period(0) {
  this->spec = {
      .format   = SDL_AUDIO_F32,
      .channels = 1,
      .freq     = int(this->samp_freq()),
  };
}

bool gb_pulsewave_channel_t::waveform_step() {
  assert(this->phase < 8);
  if ((this->duty_cycle >> this->phase) & 1) return true;
  return false;
}
double gb_pulsewave_channel_t::samp_freq() {
  /*
   *    1,048,576
   *  ------------- Hz
   *  2048 - period
   */
  GB_assert(this->period < 2048);

  return 1'048'576.0 / (2048 - this->period);
}
double gb_pulsewave_channel_t::tone_freq() {
  /*
   *     131,072
   *  ------------- Hz
   *  2048 - period
   */

  return this->samp_freq() / 8;
}

gb_apu_t::gb_apu(gb_state_t &gb_state) : parent(gb_state) {
  CheckedSDL(Init(SDL_INIT_AUDIO));

  this->output_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &this->ch1.spec, NULL, NULL);
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
    this->ch1.counter = MAX_PERIOD - this->ch1.period;
    CheckedSDL(ResumeAudioStreamDevice(this->output_stream));
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

  uint16_t old_period = this->ch1.period;
  this->ch1.period    = io_regs.nr13 | ((io_regs.nr14 & 0b0000'0111) << 8);
  if (this->ch1.period != old_period) {
    this->ch1.spec.freq = this->ch1.samp_freq();
    CheckedSDL(SetAudioStreamFormat(this->output_stream, NULL, &this->ch1.spec));
  }
  uint8_t cycle_index = ((io_regs.nr11 >> 6) & 0b11);
  switch (cycle_index) {
  case 0b00: this->ch1.duty_cycle = GB_DUTY_CYCLE_EIGHTH; break;
  case 0b01: this->ch1.duty_cycle = GB_DUTY_CYCLE_FOURTH; break;
  case 0b10: this->ch1.duty_cycle = GB_DUTY_CYCLE_HALF; break;
  case 0b11: this->ch1.duty_cycle = GB_DUTY_CYCLE_THREE_FOURTHS; break;
  default: unreachable();
  }
}

void gb_apu_t::spend_mcycles(uint16_t m_cycles) {
  for (uint16_t i = 0; i < m_cycles; i++) {
    this->tick();
  }
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
        ch.counter = MAX_PERIOD - ch.period;
        ch.phase++;
        ch.phase %= 8;

        float curr_sample = this->ch1.waveform_step() ? 1.0f : -1.0f;
        SDL_PutAudioStreamData(this->output_stream, &curr_sample, sizeof(float));
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
