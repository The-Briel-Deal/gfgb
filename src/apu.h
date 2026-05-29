#ifndef GB_APU_H
#define GB_APU_H

#include <stdint.h>

#include <SDL3/SDL_audio.h>
#ifdef __cplusplus
#include <format>
#include <limits>
#include <string>
#include <utility>
using str = std::string;
extern "C" {
#endif

#define APU_DBG_SAMPLE_BUFFER_SIZE 10'000

struct gb_state;
typedef struct gb_state gb_state_t;
typedef uint16_t        io_reg_addr_t;
typedef float           gb_apu_sample_buffer_t[APU_DBG_SAMPLE_BUFFER_SIZE];

typedef enum gb_duty_cycle : uint8_t {
  GB_DUTY_CYCLE_EIGHTH        = 0b1000'0000,
  GB_DUTY_CYCLE_FOURTH        = 0b1000'0001,
  GB_DUTY_CYCLE_HALF          = 0b1110'0001,
  GB_DUTY_CYCLE_THREE_FOURTHS = 0b0111'1110,
} gb_duty_cycle_t;

#ifdef __cplusplus
extern "C++" {
// Simple formatter specialization for the duty cycle enum, this just makes it easier print/format
template <> struct std::formatter<gb_duty_cycle_t> : formatter<std::string> {
  constexpr auto parse(std::format_parse_context &ctx) {
    return ctx.begin();
  }
  auto format(gb_duty_cycle_t duty_cycle, format_context &ctx) const {
    return std::format_to(ctx.out(), "{:#010b}", (uint8_t)duty_cycle);
  }
};
}
#endif

typedef struct gb_pulsewave_channel {
#ifdef __cplusplus
  gb_pulsewave_channel();
  bool   waveform_step();
  double samp_freq(); // How many times a second the APU changes phase
  double tone_freq(); // this->samp_freq() / 8
  void   start();
  void   stop();
  void   reset();
  void   len_tick();
  void   env_sweep_tick();
  void   period_sweep_tick();

  void    set_NRx4(uint8_t apu_div, uint8_t val);
  uint8_t get_NRx4();

  str dbg_state_str();

  static constexpr int MAX_LENGTH = 64;
#endif
  bool dbg_muted; // Set if muted via the imgui debug ui.

  bool            on;
  bool            dac_on;
  bool            left_ch_on;
  bool            right_ch_on;
  bool            length_enabled;
  gb_duty_cycle_t duty_cycle;
  uint8_t         initial_length;
  uint8_t         length;
  uint8_t         phase;
  uint16_t        counter;
  uint16_t        next_period;
  uint16_t        curr_period;

  // From `NRx2`, these don't take effect until a next trigger.
  uint8_t initial_volume;
  bool    next_env_dir;
  uint8_t next_env_sweep_pace;

  // On trigger, copy the above three fields into these 3.
  uint8_t curr_volume;
  bool    curr_env_dir;
  uint8_t curr_env_sweep_pace;

  uint8_t env_sweep_ticks;

  // TODO: Once I add channel 2 I need to add a field which indicates whether or not the channel has a period sweep.

  // From `NRx0`, these don't take effect until a next trigger.
  uint8_t next_period_sweep_pace;
  uint8_t curr_period_sweep_pace;
  // I'm struggling to find info on if these only take effect on trigger. It looks like resetting sweep direction from
  // 1->0 stops the channel however.
  uint8_t period_sweep_dir;
  uint8_t period_sweep_step;

  uint8_t period_sweep_ticks;

  // Two circular buffers of the last APU_DBG_SAMPLE_BUFFER_SIZE samples which are displayed.
  // TODO: Instead of having a buffer of 10,000 samples, I could reduce how often samples are put into this buffer.
  gb_apu_sample_buffer_t sample_buffer_left;
  gb_apu_sample_buffer_t sample_buffer_right;
} gb_pulsewave_channel_t;

typedef enum gb_ch3_volume : uint8_t {
  GB_CH3_VOLUME_MUTE = 0b00,
  GB_CH3_VOLUME_FULL = 0b01,
  GB_CH3_VOLUME_HALF = 0b10,
  GB_CH3_VOLUME_QUAR = 0b11,
} gb_ch3_volume_t;
#ifdef __cplusplus
extern "C++" {
// Simple formatter specialization for the duty cycle enum, this just makes it easier print/format
template <> struct std::formatter<gb_ch3_volume_t> : formatter<std::string> {
  constexpr auto parse(std::format_parse_context &ctx) {
    return ctx.begin();
  }
  auto format(gb_ch3_volume_t volume, format_context &ctx) const {
    switch (volume) {
      case GB_CH3_VOLUME_MUTE: return std::format_to(ctx.out(), "Mute");
      case GB_CH3_VOLUME_FULL: return std::format_to(ctx.out(), "Full");
      case GB_CH3_VOLUME_HALF: return std::format_to(ctx.out(), "Half");
      case GB_CH3_VOLUME_QUAR: return std::format_to(ctx.out(), "Quarter");
      default: std::unreachable();
    }
  }
};
}
#endif

#define IO_WAVE_PATTERN_RAM_START 0xFF30
#define IO_WAVE_PATTERN_RAM_LEN   16

typedef struct gb_wave_output_channel {
#ifdef __cplusplus
  gb_wave_output_channel();
  void start();
  void stop();
  void reset();
  void len_tick();

  void    set_NRx4(uint8_t apu_div, uint8_t val);
  uint8_t get_NRx4();

  str dbg_state_str();

  static constexpr int MAX_LENGTH = 256;
#endif
  bool dbg_muted; // Set if muted via the imgui debug ui.
  bool on;
  bool dac_on;
  bool right_ch_on;
  bool left_ch_on;

  bool     length_enabled;
  uint16_t initial_length;
  uint16_t length;

  gb_ch3_volume_t vol;

  uint16_t next_period;
  uint16_t curr_period;
  uint8_t  phase;
  int32_t  counter;

  uint8_t wave_pattern[IO_WAVE_PATTERN_RAM_LEN];

  // Two circular buffers of the last APU_DBG_SAMPLE_BUFFER_SIZE samples which are displayed.
  // TODO: Instead of having a buffer of 10,000 samples, I could reduce how often samples are put into this buffer.
  gb_apu_sample_buffer_t sample_buffer_left;
  gb_apu_sample_buffer_t sample_buffer_right;
} gb_wave_output_channel_t;

typedef struct gb_noise_channel {
#ifdef __cplusplus
  gb_noise_channel();
  void start();
  void stop();
  void reset();
  void len_tick();
  void env_sweep_tick();

  void    set_NRx4(uint8_t apu_div, uint8_t val);
  uint8_t get_NRx4();

  str dbg_state_str();

  static constexpr int MAX_LENGTH = 64;
#endif
  bool on;
  bool dac_on;
  bool dbg_muted; // Set if muted via the imgui debug ui.

  uint16_t lsfr;        // Current LSFR state.
  bool     curr_sample; // Last bit shifted out of LSFR.

  // From `NR51`
  bool left_ch_on;
  bool right_ch_on;
  // From `NRx2`, these don't take effect until a next trigger.
  uint8_t initial_volume;
  bool    next_env_dir;
  uint8_t next_env_sweep_pace;

  // On trigger, copy the above three fields into these 3.
  uint8_t curr_volume;
  bool    curr_env_dir;
  uint8_t curr_env_sweep_pace;
  uint8_t env_sweep_ticks;

  // From `NR43`
  uint8_t clock_shift;
  bool    lsfr_width;
  uint8_t clock_div;
  int     counter;

  // From `NR41` and `NR44`
  bool    length_enabled;
  uint8_t initial_length;
  uint8_t length;

  gb_apu_sample_buffer_t sample_buffer_left;
  gb_apu_sample_buffer_t sample_buffer_right;
} gb_noise_channel_t;

typedef struct gb_apu {
#ifdef __cplusplus
  // I want methods to still be able touch other parts of gameboy state like the audio registers.
  gb_apu();
  ~gb_apu();

  // We dispatch APU reg reads/writes to here so that they can be immediately parsed on write and reconstructed on read.
  // This prevents
  uint8_t read_io_reg(io_reg_addr_t reg);
  void    write_io_reg(io_reg_addr_t reg, uint8_t val);

  // Essentially just calls tick m_cycle times (m_cycle/2 times in CGB double speed once that is implemented).
  void spend_mcycles(uint16_t m_cycles);

  // Call once per cycle (1,048,576 Hz regardless of CGB double speed).
  void tick();

  // Call 512 times per second on the falling edge of div bit 4.
  void div_tick();

  // The exec speed of the emulator, stored in `gb_state->dbg.speed_factor`, used to consume audio samples faster/slower
  // if the emulator is running faster/slower.
  void set_speed(float speed);

#endif

  bool on;

  // From `NR50`
  bool    vin_left;
  bool    vin_right;
  uint8_t vol_left;
  uint8_t vol_right;

  uint8_t div;
  // The current position we are at in
  uint16_t sample_buffer_index;
#ifdef __cplusplus
  static_assert(std::numeric_limits<decltype(sample_buffer_index)>::max() >= APU_DBG_SAMPLE_BUFFER_SIZE,
                "Max val of sample_buffer_index must be greater than the size of sample buffers.");
#endif
  // Sample buffer containing the mixed result of all channels
  gb_apu_sample_buffer_t sample_buffer_left;
  gb_apu_sample_buffer_t sample_buffer_right;

  gb_pulsewave_channel_t   ch1;
  gb_pulsewave_channel_t   ch2;
  gb_wave_output_channel_t ch3;
  gb_noise_channel_t       ch4;
  SDL_AudioDeviceID        output_device;
  SDL_AudioStream         *stream;

  uint8_t sample_counter; // This is reset to `TICKS_PER_SAMPLE` every time it reaches 0. When it reaches 0 a sample is
                          // put in the queue for SDL.
} gb_apu_t;

#ifdef __cplusplus
}
#endif

#endif // GB_APU_H
