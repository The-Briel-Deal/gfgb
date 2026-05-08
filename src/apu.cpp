#include "apu.h"
#include "common.h"

#include <cmath>

#define MAX_PERIOD           (1 << 11)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8
#define TICKS_PER_SAMPLE     4

#define AUDIO_SAMPLE_FREQ (APU_CLOCK / TICKS_PER_SAMPLE)

gb_pulsewave_channel_t::gb_pulsewave_channel() {
  this->phase          = 0;
  this->counter        = MAX_PERIOD;
  this->next_period    = 0;
  this->curr_period    = 0;
  this->initial_length = 0;
  this->length         = 0;
  this->length_enabled = false;
  this->duty_cycle     = GB_DUTY_CYCLE_HALF;

  // `NRx2`
  this->initial_volume      = 0;
  this->next_env_dir        = false;
  this->next_env_sweep_pace = 0;
  this->curr_volume         = 0;
  this->curr_env_dir        = false;
  this->curr_env_sweep_pace = 0;
  this->env_sweep_ticks     = 0;

  // `NR10` (only on channel 1)
  this->next_period_sweep_pace = 0;
  this->curr_period_sweep_pace = 0;
  this->period_sweep_dir       = 0;
  this->period_sweep_step      = 0;
  this->period_sweep_ticks     = 0;

  // Audio buffer for graph in ImGui debugger.
  GB_memset(this->sample_buffer, 0, sizeof(this->sample_buffer));
  this->sample_buffer_start = 0;
  this->sample_buffer_len   = 0;
}
void gb_pulsewave_channel_t::start() {
  this->on                     = true;
  this->length                 = 64 - this->initial_length;
  this->curr_period            = this->next_period;
  this->curr_volume            = this->initial_volume;
  this->curr_env_dir           = this->next_env_dir;
  this->curr_env_sweep_pace    = this->next_env_sweep_pace;
  this->curr_period_sweep_pace = this->next_period_sweep_pace;

  this->env_sweep_ticks    = 0;
  this->period_sweep_ticks = 0;
}
void gb_pulsewave_channel_t::stop() { this->on = false; }

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
  GB_assert(this->curr_period < 2048);

  return 1'048'576.0 / (2048 - this->curr_period);
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

  this->sample_counter = TICKS_PER_SAMPLE;
  this->output_device  = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if (this->output_device == 0) {
    // TODO: I should probably handle this case gracefully since audio isn't really mandatory.
    LogError("Couldn't create audio stream: %s", SDL_GetError());
    abort();
  }

  // All gameboy channels share a single stream which we mix.
  SDL_AudioSpec spec = {
      .format   = SDL_AUDIO_F32,
      .channels = 1,
      .freq     = int(AUDIO_SAMPLE_FREQ),
  };
  this->stream = SDL_OpenAudioDeviceStream(this->output_device, &spec, NULL, NULL);
  SDL_ResumeAudioStreamDevice(this->stream);
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
    case IO_NR10: {
      uint8_t val = 0b1000'0000;
      val |= 0b0111'0000 & (this->ch1.next_period_sweep_pace << 4);
      val |= 0b0000'1000 & (this->ch1.period_sweep_dir << 3);
      val |= 0b0000'0111 & (this->ch1.period_sweep_step << 0);
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
      val |= 0b0000'0111 & (this->ch1.next_env_sweep_pace << 0);
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
    case IO_NR10: {
      this->ch1.next_period_sweep_pace = (val & 0b0111'0000) >> 4;
      this->ch1.period_sweep_dir       = (val & 0b0000'1000) >> 3;
      this->ch1.period_sweep_step      = (val & 0b0000'0111) >> 0;
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
      this->ch1.initial_volume      = (val & 0b1111'0000) >> 4;
      this->ch1.next_env_dir        = (val & 0b0000'1000) >> 3;
      this->ch1.next_env_sweep_pace = (val & 0b0000'0111) >> 0;
      if ((val & 0xF8) == 0) this->ch1.stop();
      return;
    }
    case IO_NR13: {
      this->ch1.next_period &= 0xFF00;
      this->ch1.next_period |= (val & 0x00FF);
      return;
    }
    case IO_NR14: {
      this->ch1.length_enabled = (val >> 6) & 1;
      this->ch1.next_period &= 0x00FF;
      this->ch1.next_period |= (val & 0b0000'0111) << 8;
      if ((val >> 7) & 1) { // Trigger if this bit is high
        this->ch1.start();
      }
      return;
    }

    default: LogError("Write performed on unimplemented APU IO Reg 0x%.4X", reg); return;
  }
}

void gb_apu_t::set_speed(float speed) { CheckedSDL(SetAudioStreamFrequencyRatio(this->stream, speed)); }
void gb_apu_t::spend_mcycles(uint16_t m_cycles) {
  for (uint16_t i = 0; i < m_cycles; i++) {
    this->tick();
  }
}

void gb_apu_t::tick() {
  bool apu_powered_on = (this->on);

  bool  sample_this_tick = false;
  float sample           = 0.0f;
  if (--this->sample_counter == 0) {
    sample_this_tick     = true;
    this->sample_counter = TICKS_PER_SAMPLE;
  }
  if (apu_powered_on) {
    if (this->ch1.on) {
      gb_pulsewave_channel_t &ch = this->ch1;
      // TODO: Sweep functionality (NR10)
      ch.counter--;
      if (ch.counter == 0) {
        ch.counter = MAX_PERIOD - ch.curr_period;
        ch.phase++;
        ch.phase %= 8;
      }

      if (sample_this_tick) {
        float ch1_sample = ch.waveform_step() ? 1.0f : -1.0f;
        GB_assert(ch.curr_volume < 16);
        ch1_sample *= (float(ch.curr_volume) / 16.0f);
        static constexpr int sample_buffer_size = ((sizeof(ch.sample_buffer) / sizeof(*ch.sample_buffer)));
        if (ch.sample_buffer_len >= sample_buffer_size) {
          ch.sample_buffer_len--;
          ch.sample_buffer_start++;
          ch.sample_buffer_start %= sample_buffer_size;
        }
        ch.sample_buffer[(ch.sample_buffer_start + (ch.sample_buffer_len++)) % sample_buffer_size] = ch1_sample;

        sample += ch1_sample;
      }
    }
    // TODO: Implement Channel 2
    // TODO: Implement Channel 3
    // TODO: Implement Channel 4
  }
  if (sample_this_tick) SDL_PutAudioStreamData(this->stream, &sample, sizeof(float));
}

void gb_apu_t::div_tick() {
  // See: https://gbdev.io/pandocs/Audio_details.html#div-apu
  uint8_t old_div_apu = this->div;
  this->div++;
  uint8_t new_div_apu = this->div;

  // Sound Length
  if (falling_edge_bit(0, old_div_apu, new_div_apu)) {
    if (!this->ch1.on) goto sound_len_end;
    if (this->ch1.length_enabled && !((--this->ch1.length) > 0)) {
      this->ch1.stop();
    }
  }
sound_len_end:
  // Channel 1 Freq/Period Sweep
  if (falling_edge_bit(1, old_div_apu, new_div_apu)) {
    if (!this->ch1.on) goto per_sweep_end;
    if (this->ch1.curr_period_sweep_pace == 0) goto per_sweep_end;
    this->ch1.period_sweep_ticks++;
    if (this->ch1.period_sweep_ticks < this->ch1.curr_period_sweep_pace) goto per_sweep_end;
    this->ch1.period_sweep_ticks = 0;

    int addend = (this->ch1.curr_period / (std::pow(2, this->ch1.period_sweep_step)));
    if (this->ch1.period_sweep_dir) {
      // Shouldn't be possible unless I have crazybonesitis
      GB_assert(this->ch1.curr_period >= addend);
      addend *= -1;
    }
    if ((this->ch1.curr_period + addend) > 0x7FF) {
      this->ch1.stop();
      goto per_sweep_end;
    }
    this->ch1.curr_period += addend;
  }
per_sweep_end:
  // Envelope Sweep
  if (falling_edge_bit(2, old_div_apu, new_div_apu)) {
    if (!this->ch1.on) goto env_sweep_end;
    if (this->ch1.curr_env_sweep_pace == 0) goto env_sweep_end;

    this->ch1.env_sweep_ticks++;
    if (this->ch1.env_sweep_ticks < this->ch1.curr_env_sweep_pace) goto env_sweep_end;
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
