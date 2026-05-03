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
#endif
  bool            on;
  gb_duty_cycle_t duty_cycle;
  bool            length_enabled;
  uint8_t         initial_length;
  uint8_t         length;
  uint8_t         phase;
  uint16_t        counter;
  uint16_t        period;

  // From `NR12`, these don't take effect until a next trigger.
  uint8_t initial_volume;
  bool    next_env_dir;
  bool    next_sweep_pace;

  // On trigger, copy the above three fields into these 3.
  uint8_t curr_volume;
  bool    curr_env_dir;
  bool    curr_sweep_pace;

  SDL_AudioSpec    spec;
  SDL_AudioStream *stream;
} gb_pulsewave_channel_t;

typedef struct gb_apu {
#ifdef __cplusplus
  // I want methods to still be able touch other parts of gameboy state like the audio registers.
  gb_apu(gb_state_t &gb_state);

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

private:
  // TODO: Remove this once read/write_io_reg methods are implemented.
  void sync_regs();
#endif

  bool on;

  uint8_t                div;
  gb_pulsewave_channel_t ch1;
  SDL_AudioDeviceID      output_device;

#ifdef __cplusplus
  gb_state_t &parent; // TODO: I should be able to remove this once read/write_io_reg methods are implemented.
#endif
} gb_apu_t;

#ifdef __cplusplus
}
#endif

#endif // GB_APU_H
