#include "apu.h"
#include "common.h"

#define MAX_PERIOD           (1 << 11)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8

#define AUDIO_SAMPLE_FREQ 8000
#define MIN_AUDIO_QUEUED                                                                                               \
  ((AUDIO_SAMPLE_FREQ * sizeof(float)) /                                                                               \
   60) /* We should have roughly a 60th of a second of audio queued at any given time  */

gb_pulsewave_channel_t::gb_pulsewave_channel() {
  this->phase          = 0;
  this->counter        = MAX_PERIOD;
  this->period         = 0;
  this->initial_length = 0;
  this->length         = 0;
  this->length_enabled = false;
  this->duty_cycle     = GB_DUTY_CYCLE_HALF;

  this->initial_volume  = 0;
  this->next_env_dir    = false;
  this->next_sweep_pace = 0;

  this->curr_volume     = 0;
  this->curr_env_dir    = false;
  this->curr_sweep_pace = 0;

  this->env_sweep_ticks = 0;

  this->spec = {
      .format   = SDL_AUDIO_F32,
      .channels = 1,
      .freq     = int(this->samp_freq()),
  };
}
void gb_pulsewave_channel_t::start() {
  this->on = true;
  SDL_ResumeAudioStreamDevice(this->stream);
  this->length          = 64 - this->initial_length;
  this->curr_volume     = this->initial_volume;
  this->curr_env_dir    = this->next_env_dir;
  this->curr_sweep_pace = this->next_sweep_pace;

  this->env_sweep_ticks = 0;
}
void gb_pulsewave_channel_t::stop() {
  this->on = false;
  SDL_PauseAudioStreamDevice(this->stream);
  // TODO: Identify if it would be better for me to flush instead of clear here.
  SDL_ClearAudioStream(this->stream);
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

gb_apu_t::gb_apu() {
  CheckedSDL(Init(SDL_INIT_AUDIO));

  this->output_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if (this->output_device == 0) {
    // TODO: I should probably handle this case gracefully since audio isn't really mandatory.
    LogError("Couldn't create audio stream: %s", SDL_GetError());
    abort();
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
    case IO_NR11: {
      uint8_t val = 0b0011'1111;
      switch (this->ch1.duty_cycle) {
        case GB_DUTY_CYCLE_EIGHTH: val &= (0b00 << 6); break;
        case GB_DUTY_CYCLE_FOURTH: val &= (0b01 << 6); break;
        case GB_DUTY_CYCLE_HALF: val &= (0b10 << 6); break;
        case GB_DUTY_CYCLE_THREE_FOURTHS: val &= (0b11 << 6); break;
      }
      return val;
    }
    case IO_NR12: {
      uint8_t val = 0;
      val |= 0b1111'0000 & (this->ch1.initial_volume << 4);
      val |= 0b0000'1000 & (this->ch1.next_env_dir << 3);
      val |= 0b0000'0111 & (this->ch1.next_sweep_pace << 0);
      return val;
    }
    case IO_NR13: return 0xFF; // Write only
    case IO_NR14: {
      uint8_t val = 0b1011'1111;
      val |= (this->ch1.length_enabled & 1) << 6;
      return val;
    }

    default: LogError("Read performed on unimplemented APU IO Reg 0x%.4X", reg); return 0xFF;
  }
}
void gb_apu_t::write_io_reg(io_reg_addr_t reg, uint8_t val) {
  switch (reg) {
    case IO_NR52: {
      this->on = (val >> 7) & 1;
      return;
    }
    case IO_NR11: {
      switch (((val >> 6) & 0b11)) {
        case 0b00: this->ch1.duty_cycle = GB_DUTY_CYCLE_EIGHTH; break;
        case 0b01: this->ch1.duty_cycle = GB_DUTY_CYCLE_FOURTH; break;
        case 0b10: this->ch1.duty_cycle = GB_DUTY_CYCLE_HALF; break;
        case 0b11: this->ch1.duty_cycle = GB_DUTY_CYCLE_THREE_FOURTHS; break;
        default: unreachable();
      }
      this->ch1.initial_length = (val >> 0) & 0b0011'1111;
      this->ch1.length         = 64 - this->ch1.initial_length;
      return;
    }
    case IO_NR12: {
      this->ch1.initial_volume  = (val & 0b1111'0000) >> 4;
      this->ch1.next_env_dir    = (val & 0b0000'1000) >> 3;
      this->ch1.next_sweep_pace = (val & 0b0000'0111) >> 0;
      if ((val & 0xF8) == 0) this->ch1.stop();
      return;
    }
    case IO_NR13: {
      this->ch1.period &= 0xFF00;
      this->ch1.period |= (val & 0x00FF);
      this->ch1.spec.freq = this->ch1.samp_freq();
      CheckedSDL(SetAudioStreamFormat(this->ch1.stream, &this->ch1.spec, NULL));
      return;
    }
    case IO_NR14: {
      if ((val >> 7) & 1) { // Trigger if this bit is high
        this->ch1.start();
      }
      this->ch1.length_enabled = (val >> 6) & 1;
      this->ch1.period &= 0x00FF;
      this->ch1.period |= (val & 0b0000'0111) << 8;
      this->ch1.spec.freq = this->ch1.samp_freq();
      CheckedSDL(SetAudioStreamFormat(this->ch1.stream, &this->ch1.spec, NULL));
      return;
    }

    default: LogError("Write performed on unimplemented APU IO Reg 0x%.4X", reg); return;
  }
}

void gb_apu_t::spend_mcycles(uint16_t m_cycles) {
  for (uint16_t i = 0; i < m_cycles; i++) {
    this->tick();
  }
}

void gb_apu_t::tick() {
  bool apu_powered_on = (this->on);
  if (apu_powered_on) {
    if (this->ch1.on) {
      gb_pulsewave_channel_t &ch = this->ch1;
      // TODO: Sweep functionality (NR10)
      ch.counter--;
      if (ch.counter == 0) {
        ch.counter = MAX_PERIOD - ch.period;
        ch.phase++;
        ch.phase %= 8;

        float curr_sample = this->ch1.waveform_step() ? 1.0f : -1.0f;
        GB_assert(this->ch1.curr_volume < 16);
        curr_sample *= (float(this->ch1.curr_volume) / 16.0f);
        SDL_PutAudioStreamData(this->ch1.stream, &curr_sample, sizeof(float));
      }
    }
    // TODO: Implement Channel 2
    // TODO: Implement Channel 3
    // TODO: Implement Channel 4
  }
}

void gb_apu_t::div_tick() {
  // See: https://gbdev.io/pandocs/Audio_details.html#div-apu
  uint8_t old_div_apu = this->div;
  this->div++;
  uint8_t new_div_apu = this->div;

  // Sound Length
  if (falling_edge_bit(0, old_div_apu, new_div_apu)) {
    if (this->ch1.on) {
      if (this->ch1.length_enabled && !((--this->ch1.length) > 0)) {
        this->ch1.stop();
      }
    }
  }
  // Channel 1 Freq Sweep
  if (falling_edge_bit(1, old_div_apu, new_div_apu)) {
    // TODO: Implement Freq Sweep
  }
  // Envelope Sweep
  if (falling_edge_bit(2, old_div_apu, new_div_apu)) {
    if (this->ch1.curr_sweep_pace == 0) goto env_sweep_end;

    this->ch1.env_sweep_ticks++;
    if (this->ch1.env_sweep_ticks < this->ch1.curr_sweep_pace) goto env_sweep_end;
    this->ch1.env_sweep_ticks = 0;

    if (this->ch1.curr_env_dir) {
      // Increase Vol
      if (this->ch1.curr_volume >= 15) goto env_sweep_end;
      this->ch1.curr_volume++;
    } else {
      // Decrease Vol
      if (this->ch1.curr_volume == 0) goto env_sweep_end;
      this->ch1.curr_volume--;
    }
  }
env_sweep_end:
}
