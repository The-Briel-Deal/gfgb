#include "apu.h"
#include "common.h"

#include <cmath>

#define MAX_PERIOD           (1 << 11)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8
#define TICKS_PER_SAMPLE     4

#define AUDIO_SAMPLE_FREQ (APU_CLOCK / TICKS_PER_SAMPLE)

// TODO: Test what happens if a SET instruction is used on a register that is partially write only. Since write only
// portions of registers always return high bits I think my current implementation will rewrite those write only fields
// to all high. I'm not sure how actual hardware behaves.

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
  GB_memset(this->sample_buffer_left, 0, sizeof(this->sample_buffer_left));
  GB_memset(this->sample_buffer_right, 0, sizeof(this->sample_buffer_right));
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
void gb_pulsewave_channel_t::stop() {
  this->on = false;
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

void gb_pulsewave_channel_t::len_tick() {
  if (!this->on) return;
  if (this->length_enabled && !((--this->length) > 0)) {
    this->stop();
  }
}

void gb_pulsewave_channel_t::period_sweep_tick() {
  if (!this->on) return;
  if (this->curr_period_sweep_pace == 0) return;
  this->period_sweep_ticks++;
  if (this->period_sweep_ticks < this->curr_period_sweep_pace) return;
  this->period_sweep_ticks = 0;

  int addend = (this->curr_period / (std::pow(2, this->period_sweep_step)));
  if (this->period_sweep_dir) {
    // Shouldn't be possible unless I have crazybonesitis
    GB_assert(this->curr_period >= addend);
    addend *= -1;
  }
  if ((this->curr_period + addend) > 0x7FF) {
    this->stop();
    return;
  }
  this->curr_period += addend;
}

void gb_pulsewave_channel_t::env_sweep_tick() {

  if (!this->on) return;
  if (this->curr_env_sweep_pace == 0) return;

  this->env_sweep_ticks++;
  if (this->env_sweep_ticks < this->curr_env_sweep_pace) return;
  this->env_sweep_ticks = 0;

  if (this->curr_env_dir) {
    // Increase Vol
    if (this->curr_volume >= 15) return;
    this->curr_volume++;
  } else {
    // Decrease Vol
    if (this->curr_volume == 0) return;
    this->curr_volume--;
  }
}

gb_wave_output_channel_t::gb_wave_output_channel() {
  this->on             = false;
  this->dac_on         = false;
  this->right_ch_on    = false;
  this->left_ch_on     = false;
  this->length_enabled = false;
  this->initial_length = 0;
  this->length         = 0;
  this->next_period    = 0;
  this->curr_period    = 0;
  this->vol            = GB_CH3_VOLUME_MUTE;
}
void gb_wave_output_channel_t::start() {
  // TODO: I need to figure out if triggering ch3 re-enables the DAC.
  this->on          = true;
  this->length      = 64 - this->initial_length;
  this->curr_period = this->next_period;
}
void gb_wave_output_channel_t::stop() {
  this->on = false;
}

void gb_wave_output_channel_t::len_tick() {
  if (!this->on) return;
  if (this->length_enabled && !((--this->length) > 0)) {
    this->stop();
  }
}
gb_noise_channel_t::gb_noise_channel() {
  // `NRx2`
  this->initial_volume      = 0;
  this->next_env_dir        = false;
  this->next_env_sweep_pace = 0;
  this->curr_volume         = 0;
  this->curr_env_dir        = false;
  this->curr_env_sweep_pace = 0;
}

gb_apu_t::gb_apu() {
#ifndef GFGB_NO_AUDIO
  CheckedSDL(Init(SDL_INIT_AUDIO));
#endif

  this->sample_counter      = TICKS_PER_SAMPLE;
  this->sample_buffer_index = 0;
#ifndef GFGB_NO_AUDIO
  this->output_device = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, NULL);
  if (this->output_device == 0) {
    // TODO: I should probably handle this case gracefully since audio isn't really mandatory.
    LogError("Couldn't create audio stream: %s", SDL_GetError());
    abort();
  }
#endif

  // All gameboy channels share a single stream which we mix.
  SDL_AudioSpec spec = {
      .format   = SDL_AUDIO_F32,
      .channels = 2,
      .freq     = int(AUDIO_SAMPLE_FREQ),
  };
#ifndef GFGB_NO_AUDIO
  this->stream = SDL_OpenAudioDeviceStream(this->output_device, &spec, NULL, NULL);
  SDL_ResumeAudioStreamDevice(this->stream);
#endif
}

uint8_t gb_apu_t::read_io_reg(io_reg_addr_t reg) {
  switch (reg) {
    // Global
    case IO_NR51: { // Sound Panning
      uint8_t val = 0b0000'0000;
      val |= (this->ch1.right_ch_on << 0);
      val |= (this->ch1.left_ch_on << 4);

      val |= (this->ch2.right_ch_on << 1);
      val |= (this->ch2.left_ch_on << 5);

      val |= (this->ch3.right_ch_on << 2);
      val |= (this->ch3.left_ch_on << 6);

      /** TODO: Uncomment once channel 4 is added.
       * val |= (this->ch4.right_ch_on << 3);
       * val |= (this->ch4.left_ch_on << 7);
       **/
      return val;
    }
    case IO_NR52: {
      uint8_t val = 0b0111'0000;
      val |= (this->on << 7);
      val |= (this->ch1.on << 0);
      val |= (this->ch2.on << 1);
      // TODO: Uncomment once these channels are added.
      // val |= (this->ch3.on << 2);
      // val |= (this->ch4.on << 3);
      return val;
    }

    // Channel 1
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
        case GB_DUTY_CYCLE_EIGHTH: val |= (0b00 << 6); break;
        case GB_DUTY_CYCLE_FOURTH: val |= (0b01 << 6); break;
        case GB_DUTY_CYCLE_HALF: val |= (0b10 << 6); break;
        case GB_DUTY_CYCLE_THREE_FOURTHS: val |= (0b11 << 6); break;
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

    // Channel 2
    case IO_NR21: {
      uint8_t val = 0b0011'1111;
      switch (this->ch2.duty_cycle) {
        case GB_DUTY_CYCLE_EIGHTH: val |= (0b00 << 6); break;
        case GB_DUTY_CYCLE_FOURTH: val |= (0b01 << 6); break;
        case GB_DUTY_CYCLE_HALF: val |= (0b10 << 6); break;
        case GB_DUTY_CYCLE_THREE_FOURTHS: val |= (0b11 << 6); break;
      }
      return val;
    }
    case IO_NR22: {
      uint8_t val = 0;
      val |= 0b1111'0000 & (this->ch2.initial_volume << 4);
      val |= 0b0000'1000 & (this->ch2.next_env_dir << 3);
      val |= 0b0000'0111 & (this->ch2.next_env_sweep_pace << 0);
      return val;
    }
    case IO_NR23: return 0xFF; // Write only
    case IO_NR24: {
      uint8_t val = 0b1011'1111;
      val |= (this->ch2.length_enabled & 1) << 6;
      return val;
    }

    // Channel 3
    case IO_NR30: {
      uint8_t val = 0b0111'1111;
      val |= (this->ch3.dac_on & 1) << 7;
      return val;
    }
    case IO_NR31: return 0xFF; // Write only
    case IO_NR32: {
      uint8_t val = 0b1001'1111;
      val |= (this->ch3.vol & 0b11) << 5;
      return val;
    }
    case IO_NR33: return 0xFF; // Write only
    case IO_NR34: {
      uint8_t val = 0b1011'1111;
      val |= (this->ch3.length_enabled & 1) << 6;
      return val;
    }

    // Channel 4
    case IO_NR42: {
      uint8_t val = 0;
      val |= 0b1111'0000 & (this->ch4.initial_volume << 4);
      val |= 0b0000'1000 & (this->ch4.next_env_dir << 3);
      val |= 0b0000'0111 & (this->ch4.next_env_sweep_pace << 0);
      return val;
    }

    default: LogError("Read performed on unimplemented APU IO Reg 0x%.4X", reg); return 0xFF;
  }
}
void gb_apu_t::write_io_reg(io_reg_addr_t reg, uint8_t val) {
  switch (reg) {
    // Global
    case IO_NR51: { // Sound Panning
      this->ch1.right_ch_on = (val >> 0) & 1;
      this->ch1.left_ch_on  = (val >> 4) & 1;

      this->ch2.right_ch_on = (val >> 1) & 1;
      this->ch2.left_ch_on  = (val >> 5) & 1;

      this->ch3.right_ch_on = (val >> 2) & 1;
      this->ch3.left_ch_on  = (val >> 6) & 1;

      /** TODO: Uncomment once channel 4 is added.
       * this->ch4.right_ch_on = (val >> 3) & 1;
       * this->ch4.left_ch_on  = (val >> 7) & 1;
       **/
      return;
    }
    case IO_NR52: { // Sound On/Off
      this->on = (val >> 7) & 1;
      return;
    }

    // Channel 1
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

    // Channel 2
    case IO_NR21: {
      switch (((val >> 6) & 0b11)) {
        case 0b00: this->ch2.duty_cycle = GB_DUTY_CYCLE_EIGHTH; break;
        case 0b01: this->ch2.duty_cycle = GB_DUTY_CYCLE_FOURTH; break;
        case 0b10: this->ch2.duty_cycle = GB_DUTY_CYCLE_HALF; break;
        case 0b11: this->ch2.duty_cycle = GB_DUTY_CYCLE_THREE_FOURTHS; break;
        default: unreachable();
      }
      this->ch2.initial_length = (val >> 0) & 0b0011'1111;
      this->ch2.length         = 64 - this->ch2.initial_length;
      return;
    }
    case IO_NR22: {
      this->ch2.initial_volume      = (val & 0b1111'0000) >> 4;
      this->ch2.next_env_dir        = (val & 0b0000'1000) >> 3;
      this->ch2.next_env_sweep_pace = (val & 0b0000'0111) >> 0;
      if ((val & 0xF8) == 0) this->ch2.stop();
      return;
    }
    case IO_NR23: {
      this->ch2.next_period &= 0xFF00;
      this->ch2.next_period |= (val & 0x00FF);
      return;
    }
    case IO_NR24: {
      this->ch2.length_enabled = (val >> 6) & 1;
      this->ch2.next_period &= 0x00FF;
      this->ch2.next_period |= (val & 0b0000'0111) << 8;
      if ((val >> 7) & 1) { // Trigger if this bit is high
        this->ch2.start();
      }
      return;
    }
    // Channel 3
    case IO_NR30: {
      this->ch3.dac_on = (val >> 7) & 1;
      return;
    }
    case IO_NR31: {
      this->ch3.initial_length = val;
      return;
    }
    case IO_NR32: {
      this->ch3.vol = (gb_ch3_volume_t)((val >> 5) & 0b11);
      return;
    }
    case IO_NR33: {
      this->ch3.next_period &= 0xFF00;
      this->ch3.next_period |= (val & 0x00FF);
      return;
    }
    case IO_NR34: {
      this->ch3.next_period &= 0x00FF;
      this->ch3.next_period |= (val & 0b0000'0111) << 8;
      this->ch3.length_enabled = (val >> 6) & 1;
      if ((val >> 7) & 1) { // Trigger if this bit is high
        this->ch3.start();
      }
      return;
    }

    // Channel 4
    case IO_NR42: {
      this->ch4.initial_volume      = (val & 0b1111'0000) >> 4;
      this->ch4.next_env_dir        = (val & 0b0000'1000) >> 3;
      this->ch4.next_env_sweep_pace = (val & 0b0000'0111) >> 0;
      if ((val & 0xF8) == 0) {
        // TODO: Uncomment once stop is implemented
        // this->ch4.stop();
      }
      return;
    }

    default: LogError("Write performed on unimplemented APU IO Reg 0x%.4X", reg); return;
  }
}

void gb_apu_t::set_speed(float speed) {
#ifndef GFGB_NO_AUDIO
  CheckedSDL(SetAudioStreamFrequencyRatio(this->stream, speed));
#endif
}
void gb_apu_t::spend_mcycles(uint16_t m_cycles) {
  for (uint16_t i = 0; i < m_cycles; i++) {
    this->tick();
  }
}

void gb_apu_t::tick() {
  bool apu_powered_on = (this->on);

  float  samples[2]   = {0.0f, 0.0f};
  float &left_sample  = samples[0];
  float &right_sample = samples[1];

  bool sample_this_tick = false;
  if (--this->sample_counter == 0) {
    sample_this_tick     = true;
    this->sample_counter = TICKS_PER_SAMPLE;
    this->sample_buffer_index++;
    if (this->sample_buffer_index >= APU_DBG_SAMPLE_BUFFER_SIZE) this->sample_buffer_index = 0;

    this->ch1.sample_buffer_left[this->sample_buffer_index]  = 0;
    this->ch1.sample_buffer_right[this->sample_buffer_index] = 0;

    this->ch2.sample_buffer_left[this->sample_buffer_index]  = 0;
    this->ch2.sample_buffer_right[this->sample_buffer_index] = 0;
  }
  if (apu_powered_on) {
    // Channel 1
    if (this->ch1.on) {
      gb_pulsewave_channel_t &ch = this->ch1;
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
        ch1_sample /= 4; // Divide the channels sample by 1/4th to prevent clipping when all channels are mixed together
        if (ch.left_ch_on) {
          ch.sample_buffer_left[this->sample_buffer_index] = ch1_sample;
          left_sample += ch1_sample;
        }

        if (ch.right_ch_on) {
          ch.sample_buffer_right[this->sample_buffer_index] = ch1_sample;
          right_sample += ch1_sample;
        }
      }
    }
    // Channel 2
    if (this->ch2.on) {
      gb_pulsewave_channel_t &ch = this->ch2;
      ch.counter--;
      if (ch.counter == 0) {
        ch.counter = MAX_PERIOD - ch.curr_period;
        ch.phase++;
        ch.phase %= 8;
      }

      if (sample_this_tick) {
        float ch2_sample = ch.waveform_step() ? 1.0f : -1.0f;
        GB_assert(ch.curr_volume < 16);
        ch2_sample *= (float(ch.curr_volume) / 16.0f);
        ch2_sample /= 4;
        if (ch.left_ch_on) {
          ch.sample_buffer_left[this->sample_buffer_index] = ch2_sample;
          left_sample += ch2_sample;
        }

        if (ch.right_ch_on) {
          ch.sample_buffer_right[this->sample_buffer_index] = ch2_sample;
          right_sample += ch2_sample;
        }
      }
    }
    // TODO: Implement Channel 3
    // TODO: Implement Channel 4
  }

#ifndef GFGB_NO_AUDIO
  if (sample_this_tick) SDL_PutAudioStreamData(this->stream, samples, sizeof(samples));
#endif
}

void gb_apu_t::div_tick() {
  // See: https://gbdev.io/pandocs/Audio_details.html#div-apu
  uint8_t old_div_apu = this->div;
  this->div++;
  uint8_t new_div_apu = this->div;

  // Sound Length
  if (falling_edge_bit(0, old_div_apu, new_div_apu)) {
    this->ch1.len_tick();
    this->ch2.len_tick();
  }
  // Period Sweep
  if (falling_edge_bit(1, old_div_apu, new_div_apu)) {
    // Period Sweep only on Channel 1
    this->ch1.period_sweep_tick();
  }
  // Envelope Sweep
  if (falling_edge_bit(2, old_div_apu, new_div_apu)) {
    this->ch1.env_sweep_tick();
    this->ch2.env_sweep_tick();
  }
}
