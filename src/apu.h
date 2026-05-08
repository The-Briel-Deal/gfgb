#ifndef GB_APU_H
#define GB_APU_H

#include <stdint.h>

#include <SDL3/SDL_audio.h>
#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;
typedef struct gb_state gb_state_t;
typedef uint16_t        io_reg_addr_t;

typedef enum gb_duty_cycle : uint8_t {
  GB_DUTY_CYCLE_EIGHTH        = 0b1000'0000,
  GB_DUTY_CYCLE_FOURTH        = 0b1000'0001,
  GB_DUTY_CYCLE_HALF          = 0b1110'0001,
  GB_DUTY_CYCLE_THREE_FOURTHS = 0b0111'1110,
} gb_duty_cycle_t;

typedef struct gb_pulsewave_channel {
#ifdef __cplusplus
  gb_pulsewave_channel();
  bool   waveform_step();
  double samp_freq(); // How many times a second the APU changes phase
  double tone_freq(); // this->samp_freq() / 8
  void   start();
  void   stop();
#endif
  bool            on;
  gb_duty_cycle_t duty_cycle;
  bool            length_enabled;
  uint8_t         initial_length;
  uint8_t         length;
  uint8_t         phase;
  uint16_t        counter;
  uint16_t        next_period;
  uint16_t        curr_period;

  // From `NR12`, these don't take effect until a next trigger.
  uint8_t initial_volume;
  bool    next_env_dir;
  uint8_t next_env_sweep_pace;

  // On trigger, copy the above three fields into these 3.
  uint8_t curr_volume;
  bool    curr_env_dir;
  uint8_t curr_env_sweep_pace;

  uint8_t env_sweep_ticks;

  // TODO: Once I add channel 2 I need to add a field which indicates whether or not the channel has a period sweep.

  // From `NR10`, these don't take effect until a next trigger.
  uint8_t next_period_sweep_pace;
  uint8_t curr_period_sweep_pace;
  // I'm struggling to find info on if these only take effect on trigger. It looks like resetting sweep direction from
  // 1->0 stops the channel however.
  uint8_t period_sweep_dir;
  uint8_t period_sweep_step;

  uint8_t period_sweep_ticks;

  SDL_AudioSpec    spec;
  SDL_AudioStream *stream;

  // A circular buffer of the last n samples which are displayed.
  int   sample_buffer_start;
  int   sample_buffer_len;
  float sample_buffer[1000];
} gb_pulsewave_channel_t;

typedef struct gb_apu {
#ifdef __cplusplus
  // I want methods to still be able touch other parts of gameboy state like the audio registers.
  gb_apu();

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

  uint8_t                div;
  gb_pulsewave_channel_t ch1;
  SDL_AudioDeviceID      output_device;
  uint8_t sample_counter; // This is reset to `TICKS_PER_SAMPLE` every time it reaches 0. When it reaches 0 a sample is
                          // put in the queue for SDL.
} gb_apu_t;

#ifdef __cplusplus
}
#endif

#endif // GB_APU_H
