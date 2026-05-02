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

gb_pulsewave_channel_t::gb_pulsewave_channel() {
  this->phase          = 0;
  this->counter        = MAX_PERIOD;
  this->period         = 0;
  this->length         = 0;
  this->length_enabled = false;

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

  this->output_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if (this->output_device == 0) {
    Err((&gb_state), "Couldn't create audio stream: %s", SDL_GetError());
  }

  this->ch1.stream = SDL_OpenAudioDeviceStream(this->output_device, &this->ch1.spec, NULL, NULL);
}

uint8_t gb_apu_t::read_io_reg(io_reg_addr_t reg) {
  switch (reg) {
  case IO_NR52: {
    uint8_t val = 0b0111'0000;
    val |= (this->on << 7);
    val |= (this->ch1.on << 0);
    // TODO: Uncomment once these channels are added.
    // val |= (this->ch2.on << 1);
    // val |= (this->ch3.on << 2);
    // val |= (this->ch4.on << 3);
    return val;
  }
  case IO_NR13: return 0xFF; // Write only
  case IO_NR14: {
    uint8_t val = 0b1011'1111;
    val |= (this->ch1.length_enabled & 1) << 6;
    return val;
  }

  default: unreachable();
  }
}
void gb_apu_t::write_io_reg(io_reg_addr_t reg, uint8_t val) {
  switch (reg) {
  case IO_NR52: {
    this->on = (val >> 7) & 1;
    return;
  }
  case IO_NR13: {
    this->ch1.period &= 0xFF00;
    this->ch1.period |= (val & 0x00FF);
    return;
  }
  case IO_NR14: {
    if ((val >> 7) & 1) this->ch1.on = true;
    this->ch1.length_enabled = (val >> 6) & 1;
    this->ch1.period &= 0x00FF;
    this->ch1.period |= (val & 0b0000'0111) << 8;
    return;
  }

  default: unreachable();
  }
}

void gb_apu_t::sync_regs() {
  io_regs_t &io_regs = this->parent.saved.regs.io;

  // Channel 1
  bool     ch1_triggered = (io_regs.nr14 >> 7) & 1;
  uint16_t old_period    = this->ch1.period;
  this->ch1.period       = io_regs.nr13 | ((io_regs.nr14 & 0b0000'0111) << 8);
  if (this->ch1.period != old_period) {
    this->ch1.spec.freq = this->ch1.samp_freq();
    CheckedSDL(SetAudioStreamFormat(this->ch1.stream, &this->ch1.spec, NULL));
  }
  if (ch1_triggered) {
    io_regs.nr52 |= (1 << 0);
    io_regs.nr14 &= ~(1 << 7);
    this->ch1.length_enabled = (io_regs.nr14 >> 6) & 1;
    if (this->ch1.length_enabled) {
      uint8_t initial_length = (io_regs.nr11 >> 0) & 0b0011'1111;
      this->ch1.length       = 64 - initial_length;
    }

    this->ch1.counter = MAX_PERIOD - this->ch1.period;
    CheckedSDL(ResumeAudioStreamDevice(this->ch1.stream));
  }
  uint8_t cycle_index = ((io_regs.nr11 >> 6) & 0b11);
  switch (cycle_index) {
  case 0b00: this->ch1.duty_cycle = GB_DUTY_CYCLE_EIGHTH; break;
  case 0b01: this->ch1.duty_cycle = GB_DUTY_CYCLE_FOURTH; break;
  case 0b10: this->ch1.duty_cycle = GB_DUTY_CYCLE_HALF; break;
  case 0b11: this->ch1.duty_cycle = GB_DUTY_CYCLE_THREE_FOURTHS; break;
  default: unreachable();
  }

  // Channel 2
  bool ch2_triggered = (io_regs.nr24 >> 7) & 1;
  if (ch2_triggered) {
    io_regs.nr52 |= (1 << 1);
    io_regs.nr24 &= ~(1 << 7);
  }

  // Channel 3
  bool ch3_triggered = (io_regs.nr34 >> 7) & 1;
  if (ch3_triggered) {
    io_regs.nr52 |= (1 << 2);
    io_regs.nr34 &= ~(1 << 7);
  }

  // Channel 4
  bool ch4_triggered = (io_regs.nr44 >> 7) & 1;
  if (ch4_triggered) {
    io_regs.nr52 |= (1 << 3);
    io_regs.nr44 &= ~(1 << 7);
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
        SDL_PutAudioStreamData(this->ch1.stream, &curr_sample, sizeof(float));
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

void gb_apu_t::div_tick() {
  // See: https://gbdev.io/pandocs/Audio_details.html#div-apu
  io_regs_t &io_regs = this->parent.saved.regs.io;

  uint8_t old_div_apu = this->div;
  this->div++;
  uint8_t new_div_apu = this->div;

  // Sound Length
  if (falling_edge_bit(0, old_div_apu, new_div_apu)) {
    if (CH_IS_ON(1)) {
      if (this->ch1.length_enabled && !((--this->ch1.length) > 0)) {
        // TODO: I should probably make a helper method to play/pause a channel, i'll have to make sure I keep nr52 in
        // sync with the channel however.

        // Turn off channel 1.
        io_regs.nr52 &= ~(1 << 0);
        SDL_PauseAudioStreamDevice(this->ch1.stream);
        // TODO: Identify if it would be better for me to flush instead of clear here.
        SDL_ClearAudioStream(this->ch1.stream);
      }
    }
  }
  // Channel 1 Freq Sweep
  if (falling_edge_bit(1, old_div_apu, new_div_apu)) {
    // TODO: Impl Freq Sweep
  }
  // Envelope Sweep
  if (falling_edge_bit(2, old_div_apu, new_div_apu)) {
    // TODO: Impl Envelope Sweep
  }
}
