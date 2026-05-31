#include "apu.h"
#include "common.h"

#include <cmath>
#include <print>
#include <sstream>

#define MAX_PERIOD           (1 << 11)
#define APU_CLOCK            DMG_CLOCK_HZ / 4
#define SAMPLES_PER_WAVEFORM 8
#define TICKS_PER_SAMPLE     4

#define AUDIO_SAMPLE_FREQ (APU_CLOCK / TICKS_PER_SAMPLE)

// TODO: Test what happens if a SET instruction is used on a register that is partially write only. Since write only
// portions of registers always return high bits I think my current implementation will rewrite those write only fields
// to all high. I'm not sure how actual hardware behaves.

gb_pulsewave_channel_t::gb_pulsewave_channel(bool has_period_sweep_unit)
    : has_period_sweep_unit(has_period_sweep_unit) {
  this->dbg_muted = false;
  this->reset();
}
void gb_pulsewave_channel_t::start() {
  this->on = this->dac_on;
  if (this->length == 0) {
    this->length = 64;
  }
  this->curr_volume         = this->initial_volume;
  this->curr_env_dir        = this->next_env_dir;
  this->curr_env_sweep_pace = this->next_env_sweep_pace;

  this->env_sweep_ticks = 0;
  if (this->has_period_sweep_unit) {
    this->period_sweep_trigger();
  }
}

// This should only be called on channel 1
void gb_pulsewave_channel_t::period_sweep_trigger() {
  this->period_sweep_shadow_period = this->period;
  this->period_sweep_timer         = this->period_sweep_pace;
  this->period_sweep_enabled       = (this->period_sweep_pace != 0) || (this->period_sweep_step != 0);
  if (this->period_sweep_step != 0) {
    // TODO: Pandocs reads like I should only be doing the overflow check so I don't think I should set the shadow reg
    // here. It wouldn't hurt to verify though.
  }
}
int gb_pulsewave_channel_t::period_sweep_calculate() {
  GB_assert(this->period_sweep_step != 0);
  int result = this->period_sweep_shadow_period;
  result >>= this->period_sweep_step;
  if (this->period_sweep_dir) {
    result *= -1;
  }
  return this->period_sweep_shadow_period + result;
}
void gb_pulsewave_channel_t::period_sweep_check() {
  int new_period = this->period_sweep_calculate();
  GB_assert(new_period >= 0); // Something has gone very wrong if the new period is less than 0.
  if (new_period >= 2048) this->stop();
}

void gb_pulsewave_channel_t::stop() {
  this->on = false;
}
void gb_pulsewave_channel_t::reset() {
  this->stop();
  this->dac_on         = false;
  this->left_ch_on     = false;
  this->right_ch_on    = false;
  this->phase          = 0;
  this->counter        = MAX_PERIOD;
  this->period         = 0;
  this->initial_length = 0;
  this->length         = 0;
  this->length_enabled = false;
  this->duty_cycle     = GB_DUTY_CYCLE_EIGHTH;

  // `NRx2`
  this->initial_volume      = 0;
  this->next_env_dir        = false;
  this->next_env_sweep_pace = 0;
  this->curr_volume         = 0;
  this->curr_env_dir        = false;
  this->curr_env_sweep_pace = 0;
  this->env_sweep_ticks     = 0;

  // `NR10` (only on channel 1)
  this->period_sweep_pace  = 0;
  this->period_sweep_dir   = 0;
  this->period_sweep_step  = 0;
  this->period_sweep_timer = 0;

  this->period_sweep_enabled       = false;
  this->period_sweep_timer         = 0;
  this->period_sweep_shadow_period = 0;

  // Audio buffer for graph in ImGui debugger.
  GB_memset(this->sample_buffer_left, 0, sizeof(this->sample_buffer_left));
  GB_memset(this->sample_buffer_right, 0, sizeof(this->sample_buffer_right));
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

void gb_pulsewave_channel_t::len_tick() {
  if (this->length_enabled) {
    if (this->length > 0) this->length--;
    if (this->length == 0) this->stop();
  }
}

void gb_pulsewave_channel_t::period_sweep_tick() {
  GB_assert(this->on);                     // This should never be called if the channel is off.
  GB_assert(this->period_sweep_enabled);
  GB_assert(this->period_sweep_pace > 0);  // This should never be called if the sweep pace is 0.
  GB_assert(this->period_sweep_timer > 0); // This should never be called with a timer already equal to 0. If that
                                           // happens the timer will underflow and rollover.
  if (--this->period_sweep_timer == 0) {
    this->period_sweep_timer = this->period_sweep_pace;
  } else {
    return;
  }

  this->period_sweep_check();
  if (this->on) this->period = this->period_sweep_calculate();
  this->period_sweep_check();
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

template <typename T> void set_NRx4(T ch, uint8_t apu_div, uint8_t val) {

  /// From https://gbdev.io/pandocs/Audio_details.html#obscure-behavior:
  //
  // Extra length clocking occurs when writing to NRx4 when the DIV-APU next step is one that doesn’t clock the
  // length timer. In this case, if the length timer was PREVIOUSLY disabled and now enabled and the length timer is
  // not zero, it is decremented. If this decrement makes it zero and trigger is clear, the channel is disabled. On
  // the CGB-02, the length timer only has to have been disabled before; the current length enable state doesn’t
  // matter. This breaks at least one game (Prehistorik Man), and was fixed on CGB-04 and CGB-05.

  bool prev_length_enabled = ch->length_enabled;
  ch->length_enabled       = (val >> 6) & 1;
  if ((!prev_length_enabled) && !falling_edge_bit(0, apu_div, (apu_div + 1))) {
    ch->len_tick();
  }
  ch->period &= 0x00FF;
  ch->period |= (val & 0b0000'0111) << 8;
  if ((val >> 7) & 1) { // Trigger if this bit is high
    ch->start();
    /// From https://gbdev.io/pandocs/Audio_details.html#obscure-behavior:
    //
    // If a channel is triggered when the DIV-APU next step is one that doesn’t clock the length timer and the
    // length timer is now enabled and length is being set to 64 (256 for wave channel) because it was previously
    // zero, it is set to 63 instead (255 for wave channel).
    if (ch->length == ch->MAX_LENGTH) {
      if (!falling_edge_bit(0, apu_div, (apu_div + 1))) {
        ch->len_tick();
      }
    }
  }
}
void gb_pulsewave_channel_t::set_NRx4(uint8_t apu_div, uint8_t val) {
  ::set_NRx4(this, apu_div, val);
}

template <typename T> uint8_t get_NRx4(T ch) {
  uint8_t val = 0b1011'1111;
  val |= (ch->length_enabled & 1) << 6;
  return val;
}

uint8_t gb_pulsewave_channel_t::get_NRx4() {
  return ::get_NRx4(this);
}

str gb_pulsewave_channel_t::dbg_state_str() {
  std::stringstream state_stringstream;

#define show_field(name, fmt) std::println(state_stringstream, #name ": " fmt, this->name)

  {
    show_field(dbg_muted, "{}");
    show_field(on, "{}");
    show_field(left_ch_on, "{}");
    show_field(right_ch_on, "{}");
    show_field(length_enabled, "{}");
    show_field(duty_cycle, "{}");
    show_field(initial_length, "{}");
    show_field(length, "{}");
    show_field(phase, "{}");
    show_field(counter, "{}");
    show_field(period, "{}");
    show_field(initial_volume, "{}");
    show_field(next_env_dir, "{}");
    show_field(next_env_sweep_pace, "{}");
    show_field(curr_volume, "{}");
    show_field(curr_env_dir, "{}");
    show_field(curr_env_sweep_pace, "{}");
    show_field(env_sweep_ticks, "{}");
    show_field(period_sweep_pace, "{}");
    show_field(period_sweep_dir, "{}");
    show_field(period_sweep_step, "{}");
    show_field(period_sweep_timer, "{}");
  }
  return state_stringstream.str();
}

gb_wave_output_channel_t::gb_wave_output_channel() {
  this->dbg_muted = false;
  this->reset();
  GB_memset(this->wave_pattern, 0, sizeof(this->wave_pattern));
  GB_memset(this->sample_buffer_left, 0, sizeof(this->sample_buffer_left));
  GB_memset(this->sample_buffer_right, 0, sizeof(this->sample_buffer_right));
}

void gb_wave_output_channel_t::start() {
  this->on = this->dac_on;
  if (this->length == 0) {
    this->length = 256;
  }
}

void gb_wave_output_channel_t::stop() {
  this->on = false;
}

void gb_wave_output_channel_t::reset() {
  this->stop();
  this->dac_on         = false;
  this->right_ch_on    = false;
  this->left_ch_on     = false;
  this->length_enabled = false;
  this->initial_length = 0;
  // TODO: Investigate when length timers aren't reset
  // (https://gbdev.io/pandocs/Audio_Registers.html#footnote-dmg_apu_off)
  this->length  = 0;
  this->period  = 0;
  this->counter = MAX_PERIOD;
  this->vol     = GB_CH3_VOLUME_MUTE;
}

void gb_wave_output_channel_t::len_tick() {
  if (this->length_enabled) {
    if (this->length > 0) this->length--;
    if (this->length == 0) this->stop();
  }
}

uint8_t gb_wave_output_channel_t::get_NRx4() {
  return ::get_NRx4(this);
}
void gb_wave_output_channel_t::set_NRx4(uint8_t apu_div, uint8_t val) {
  ::set_NRx4(this, apu_div, val);
}

str gb_wave_output_channel_t::dbg_state_str() {
  std::stringstream state_stringstream;

#define show_field(name, fmt) std::println(state_stringstream, #name ": " fmt, this->name)

  {
    show_field(dbg_muted, "{}");
    show_field(on, "{}");
    show_field(dac_on, "{}");
    show_field(right_ch_on, "{}");
    show_field(left_ch_on, "{}");
    show_field(length_enabled, "{}");
    show_field(initial_length, "{}");
    show_field(length, "{}");
    show_field(vol, "{}");
    show_field(period, "{}");
    show_field(phase, "{}");
    show_field(counter, "{}");
    show_field(wave_pattern, "{}");
  }
  return state_stringstream.str();
}

gb_noise_channel_t::gb_noise_channel() {
  this->dbg_muted = false;
  this->reset();
}

void gb_noise_channel_t::start() {
  this->on      = this->dac_on;
  this->lsfr    = 0;
  this->counter = 0;
  if (this->length == 0) {
    this->length = 64;
  }

  this->curr_volume         = this->initial_volume;
  this->curr_env_dir        = this->next_env_dir;
  this->curr_env_sweep_pace = this->next_env_sweep_pace;
  this->env_sweep_ticks     = 0;
}

void gb_noise_channel_t::stop() {
  this->on = false;
}

void gb_noise_channel_t::reset() {
  this->on      = false;
  this->dac_on  = false;
  this->lsfr    = 0;
  this->counter = 0;
  // `NR51`
  this->left_ch_on  = false;
  this->right_ch_on = false;
  // `NRx2`
  this->initial_volume      = 0;
  this->next_env_dir        = false;
  this->next_env_sweep_pace = 0;
  this->curr_volume         = 0;
  this->curr_env_dir        = false;
  this->curr_env_sweep_pace = 0;
  this->env_sweep_ticks     = 0;

  // From `NR43`
  this->clock_shift = 0;
  this->lsfr_width  = false;
  this->clock_div   = 0;

  // From `NR41` and `NR44`
  this->length_enabled = false;
  this->initial_length = 0;
  this->length         = 0;
}

void gb_noise_channel_t::len_tick() {
  if (this->length_enabled) {
    if (this->length > 0) this->length--;
    if (this->length == 0) this->stop();
  }
}

void gb_noise_channel_t::env_sweep_tick() {
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

uint8_t gb_noise_channel_t::get_NRx4() {
  return ::get_NRx4(this);
}
// Mostly the same as the template all the other channels use, except there is no period portion so I couldn't use the
// template for setting NR44.
void gb_noise_channel_t::set_NRx4(uint8_t apu_div, uint8_t val) {
  bool prev_length_enabled = this->length_enabled;
  this->length_enabled     = (val >> 6) & 1;
  if ((!prev_length_enabled) && !falling_edge_bit(0, apu_div, (apu_div + 1))) {
    this->len_tick();
  }
  if ((val >> 7) & 1) { // Trigger if this bit is high
    this->start();
    if (this->length == this->MAX_LENGTH) {
      if (!falling_edge_bit(0, apu_div, (apu_div + 1))) {
        this->len_tick();
      }
    }
  }
}

str gb_noise_channel_t::dbg_state_str() {
  std::stringstream state_stringstream;

#define show_field(name, fmt) std::println(state_stringstream, #name ": " fmt, this->name)

  {
    show_field(on, "{}");
    show_field(dbg_muted, "{}");

    show_field(lsfr, "{:#018b}");
    show_field(curr_sample, "{}");

    show_field(left_ch_on, "{}");
    show_field(right_ch_on, "{}");

    show_field(initial_volume, "{}");
    show_field(next_env_dir, "{}");
    show_field(next_env_sweep_pace, "{}");

    show_field(curr_volume, "{}");
    show_field(curr_env_dir, "{}");
    show_field(curr_env_sweep_pace, "{}");
    show_field(env_sweep_ticks, "{}");

    show_field(clock_shift, "{}");
    show_field(lsfr_width, "{}");
    show_field(clock_div, "{}");
    show_field(counter, "{}");

    show_field(length_enabled, "{}");
    show_field(initial_length, "{}");
    show_field(length, "{}");
  }
  return state_stringstream.str();
}

gb_apu_t::gb_apu() : ch1(true), ch2(false) {
#ifndef GFGB_NO_AUDIO
  CheckedSDL(Init(SDL_INIT_AUDIO));
#endif
  this->on = false;

  this->vin_left  = false;
  this->vin_right = false;
  this->vol_left  = 0;
  this->vol_right = 0;

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
  GB_memset(this->sample_buffer_left, 0, sizeof(this->sample_buffer_left));
  GB_memset(this->sample_buffer_right, 0, sizeof(this->sample_buffer_right));
}

gb_apu_t::~gb_apu() {
  SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

uint8_t gb_apu_t::read_io_reg(io_reg_addr_t reg) {
  switch (reg) {
    // Global
    case IO_NR50: { // Master Volume and VIN Panning
      uint8_t val = 0b0000'0000;
      val |= (this->vin_left << 7);
      val |= (this->vol_left << 4);
      val |= (this->vin_right << 3);
      val |= (this->vol_right << 0);
      return val;
    }
    case IO_NR51: { // Sound Panning
      uint8_t val = 0b0000'0000;
      val |= (this->ch1.right_ch_on << 0);
      val |= (this->ch1.left_ch_on << 4);

      val |= (this->ch2.right_ch_on << 1);
      val |= (this->ch2.left_ch_on << 5);

      val |= (this->ch3.right_ch_on << 2);
      val |= (this->ch3.left_ch_on << 6);

      val |= (this->ch4.right_ch_on << 3);
      val |= (this->ch4.left_ch_on << 7);
      return val;
    }
    case IO_NR52: {
      uint8_t val = 0b0111'0000;
      val |= (this->on << 7);
      val |= (this->ch1.on << 0);
      val |= (this->ch2.on << 1);
      val |= (this->ch3.on << 2);
      val |= (this->ch4.on << 3);
      return val;
    }

    // Channel 1
    case IO_NR10: {
      uint8_t val = 0b1000'0000;
      val |= 0b0111'0000 & (this->ch1.period_sweep_pace << 4);
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
      return this->ch1.get_NRx4();
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
      return this->ch2.get_NRx4();
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
      return this->ch3.get_NRx4();
    }

    // Channel 4
    case IO_NR41: return 0xFF; // Write only
    case IO_NR42: {
      uint8_t val = 0;
      val |= 0b1111'0000 & (this->ch4.initial_volume << 4);
      val |= 0b0000'1000 & (this->ch4.next_env_dir << 3);
      val |= 0b0000'0111 & (this->ch4.next_env_sweep_pace << 0);
      return val;
    }
    case IO_NR43: {
      uint8_t val = 0;
      val |= 0b1111'0000 & (this->ch4.clock_shift << 4);
      val |= 0b0000'1000 & (this->ch4.lsfr_width << 3);
      val |= 0b0000'0111 & (this->ch4.clock_div << 0);
      return val;
    }
    case IO_NR44: {
      return this->ch4.get_NRx4();
    }

    default: {
      if (reg >= IO_WAVE_PATTERN_RAM_START && reg < (IO_WAVE_PATTERN_RAM_START + IO_WAVE_PATTERN_RAM_LEN)) {
        uint8_t index = reg - IO_WAVE_PATTERN_RAM_START;
        GB_assert(index < IO_WAVE_PATTERN_RAM_LEN);

        return this->ch3.wave_pattern[index];
      }
      LogError("Read performed on unimplemented APU IO Reg 0x%.4X", reg);
      return 0xFF;
    }
  }
}

void gb_apu_t::write_io_reg(io_reg_addr_t reg, uint8_t val) {
  if (!this->on && reg != IO_NR52) return;
  switch (reg) {
    // Global
    case IO_NR50: { // Master Volume and VIN Panning
      this->vin_left  = (val & 0b1000'0000) >> 7;
      this->vol_left  = (val & 0b0111'0000) >> 4;
      this->vin_right = (val & 0b0000'1000) >> 3;
      this->vol_right = (val & 0b0000'0111) >> 0;
      return;
    }
    case IO_NR51: { // Sound Panning
      this->ch1.right_ch_on = (val >> 0) & 1;
      this->ch1.left_ch_on  = (val >> 4) & 1;

      this->ch2.right_ch_on = (val >> 1) & 1;
      this->ch2.left_ch_on  = (val >> 5) & 1;

      this->ch3.right_ch_on = (val >> 2) & 1;
      this->ch3.left_ch_on  = (val >> 6) & 1;

      this->ch4.right_ch_on = (val >> 3) & 1;
      this->ch4.left_ch_on  = (val >> 7) & 1;
      return;
    }
    case IO_NR52: { // Sound On/Off
      this->on = (val >> 7) & 1;
      if (!this->on) {
        // TODO: If audio is turned off via NR52 bit 7 all APU registers are cleared, it appears that this includes
        // turning off the individual channels but I haven't verified this on real hardware yet.

        this->vin_left  = false;
        this->vin_right = false;
        this->vol_left  = 0;
        this->vol_right = 0;
        this->ch1.reset();
        this->ch2.reset();
        this->ch3.reset();
        this->ch4.reset();
      }
      return;
    }

    // Channel 1
    case IO_NR10: {
      this->ch1.period_sweep_pace = (val & 0b0111'0000) >> 4;
      this->ch1.period_sweep_dir  = (val & 0b0000'1000) >> 3;
      this->ch1.period_sweep_step = (val & 0b0000'0111) >> 0;
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
      this->ch1.dac_on              = (val & 0b1111'1000) != 0;
      if (!this->ch1.dac_on) this->ch1.stop();
      return;
    }
    case IO_NR13: {
      this->ch1.period &= 0xFF00;
      this->ch1.period |= (val & 0x00FF);
      return;
    }
    case IO_NR14: {
      this->ch1.set_NRx4(this->div, val);
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
      this->ch2.dac_on              = (val & 0b1111'1000) != 0;
      if (!this->ch2.dac_on) this->ch2.stop();
      return;
    }
    case IO_NR23: {
      this->ch2.period &= 0xFF00;
      this->ch2.period |= (val & 0x00FF);
      return;
    }
    case IO_NR24: {
      this->ch2.set_NRx4(this->div, val);
      return;
    }
    // Channel 3
    case IO_NR30: {
      this->ch3.dac_on = (val >> 7) & 1;
      if (!this->ch3.dac_on) this->ch3.stop();
      return;
    }
    case IO_NR31: {
      this->ch3.initial_length = val;
      this->ch3.length         = 256 - this->ch3.initial_length;
      return;
    }
    case IO_NR32: {
      this->ch3.vol = (gb_ch3_volume_t)((val >> 5) & 0b11);
      return;
    }
    case IO_NR33: {
      this->ch3.period &= 0xFF00;
      this->ch3.period |= (val & 0x00FF);
      return;
    }
    case IO_NR34: {
      this->ch3.set_NRx4(this->div, val);
      return;
    }

    // Channel 4
    case IO_NR41: {
      this->ch4.initial_length = (val >> 0) & 0b0011'1111;
      this->ch4.length         = 64 - this->ch4.initial_length;
      return;
    }
    case IO_NR42: {
      this->ch4.initial_volume      = (val & 0b1111'0000) >> 4;
      this->ch4.next_env_dir        = (val & 0b0000'1000) >> 3;
      this->ch4.next_env_sweep_pace = (val & 0b0000'0111) >> 0;
      this->ch4.dac_on              = (val & 0b1111'1000) != 0;
      if (!this->ch4.dac_on) this->ch4.stop();
      return;
    }
    case IO_NR43: {
      this->ch4.clock_shift = (val & 0b1111'0000) >> 4;
      if (this->ch4.clock_shift >= 14) {
        this->ch4.stop();
      }
      this->ch4.lsfr_width = (val & 0b0000'1000) >> 3;
      this->ch4.clock_div  = (val & 0b0000'0111) >> 0;
      return;
    }
    case IO_NR44: {
      this->ch4.set_NRx4(this->div, val);
      return;
    }

    default: {
      if (reg >= IO_WAVE_PATTERN_RAM_START && reg < (IO_WAVE_PATTERN_RAM_START + IO_WAVE_PATTERN_RAM_LEN)) {
        uint8_t index = reg - IO_WAVE_PATTERN_RAM_START;
        GB_assert(index < IO_WAVE_PATTERN_RAM_LEN);

        this->ch3.wave_pattern[index] = val;
        return;
      }
      LogError("Write performed on unimplemented APU IO Reg 0x%.4X", reg);
      return;
    }
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
  GB_assert(!this->ch2.period_sweep_enabled);
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

    this->ch3.sample_buffer_left[this->sample_buffer_index]  = 0;
    this->ch3.sample_buffer_right[this->sample_buffer_index] = 0;

    this->ch4.sample_buffer_left[this->sample_buffer_index]  = 0;
    this->ch4.sample_buffer_right[this->sample_buffer_index] = 0;

    this->sample_buffer_left[this->sample_buffer_index]  = 0;
    this->sample_buffer_right[this->sample_buffer_index] = 0;
  }
  if (apu_powered_on) {
    // Channel 1
    if (this->ch1.on) {
      gb_pulsewave_channel_t &ch = this->ch1;
      ch.counter--;
      if (ch.counter == 0) {
        ch.counter = MAX_PERIOD - ch.period;
        ch.phase++;
        ch.phase %= 8;
      }

      if (sample_this_tick && !ch.dbg_muted) {
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
        ch.counter = MAX_PERIOD - ch.period;
        ch.phase++;
        ch.phase %= 8;
      }

      if (sample_this_tick && !ch.dbg_muted) {
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
    // Channel 3
    if (this->ch3.on) {
      // The Channel 3 period timer ticks once for every 2 t-cycles (aka half of one m-cycle). Since this tick
      // function is called for every m-cycle we need to increment the counter by two every tick.
      gb_wave_output_channel_t &ch = this->ch3;
      ch.counter -= 2;
      if (ch.counter <= 0) {
        ch.counter += (MAX_PERIOD - ch.period);
        ch.phase++;
        ch.phase %= 32;
      }

      if (sample_this_tick && !ch.dbg_muted) {
        uint8_t ch3_sample_i = ch.wave_pattern[(int)(ch.phase / 2)];
        if ((ch.phase & 1) == 0) ch3_sample_i >>= 4;
        ch3_sample_i &= 0x0F;
        float ch3_sample;
        switch (ch.vol) {
          case GB_CH3_VOLUME_MUTE: ch3_sample = 0.0f; break;
          case GB_CH3_VOLUME_FULL: ch3_sample = (float(ch3_sample_i >> 0) / 15.0f) - 0.5f; break;
          case GB_CH3_VOLUME_HALF: ch3_sample = (float(ch3_sample_i >> 1) / 15.0f) - 0.25f; break;
          case GB_CH3_VOLUME_QUAR: ch3_sample = (float(ch3_sample_i >> 2) / 15.0f) - 0.125f; break;
        }
        ch3_sample /= 2;
        if (ch.left_ch_on) {
          ch.sample_buffer_left[this->sample_buffer_index] = ch3_sample;
          left_sample += ch3_sample;
        }

        if (ch.right_ch_on) {
          ch.sample_buffer_right[this->sample_buffer_index] = ch3_sample;
          right_sample += ch3_sample;
        }
      }
    }
    // Channel 4
    if (this->ch4.on) {
      gb_noise_channel_t &ch = this->ch4;
      ch.counter--;
      if (ch.counter <= 0) {
        /*
         *           262,144
         *  ----------------------- Hz
         *  clock_divider x 2^shift
         */
        int period = ch.clock_div * std::exp2(ch.clock_shift) * int((APU_CLOCK) / 262'144);

        ch.counter += period;
        bool bit0 = (ch.lsfr >> 0) & 1;
        bool bit1 = (ch.lsfr >> 1) & 1;
        if (ch.lsfr_width) {
          // 7 Bit LSFR
          ch.lsfr &= 0b0000'0000'0111'1111;
          if (bit0 == bit1) {
            ch.lsfr |= 0b0000'0000'1000'0000;
          }
        } else {
          // 15 Bit LSFR
          ch.lsfr &= 0b0111'1111'1111'1111;
          if (bit0 == bit1) {
            ch.lsfr |= 0b1000'0000'0000'0000;
          }
        }
        ch.lsfr >>= 1;
        ch.curr_sample = bit0;
      }

      if (sample_this_tick && !ch.dbg_muted) {
        float ch4_sample = -1.0f;
        if (ch.curr_sample) {
          ch4_sample = 1.0f;
        }
        ch4_sample /= 4;
        ch4_sample *= (float(ch.curr_volume) / 16.0f);
        if (ch.left_ch_on) {
          ch.sample_buffer_left[this->sample_buffer_index] = ch4_sample;
          left_sample += ch4_sample;
        }

        if (ch.right_ch_on) {
          ch.sample_buffer_right[this->sample_buffer_index] = ch4_sample;
          right_sample += ch4_sample;
        }
      }
    }
  }

  if (sample_this_tick) {
    this->sample_buffer_left[this->sample_buffer_index]  = left_sample;
    this->sample_buffer_right[this->sample_buffer_index] = right_sample;
#ifndef GFGB_NO_AUDIO
    SDL_PutAudioStreamData(this->stream, samples, sizeof(samples));
#endif
  }
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
    this->ch3.len_tick();
    this->ch4.len_tick();
  }
  // Period Sweep
  if (falling_edge_bit(1, old_div_apu, new_div_apu)) {
    // Period Sweep only on Channel 1
    if (this->ch1.on && this->ch1.period_sweep_enabled) {
      this->ch1.period_sweep_tick();
    }
  }
  // Envelope Sweep
  if (falling_edge_bit(2, old_div_apu, new_div_apu)) {
    this->ch1.env_sweep_tick();
    this->ch2.env_sweep_tick();
    this->ch4.env_sweep_tick();
  }
}
