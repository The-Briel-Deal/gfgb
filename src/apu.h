#ifndef GB_APU_H
#define GB_APU_H

#include <SDL3/SDL_audio.h>
#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;
typedef struct gb_state gb_state_t;

typedef struct gb_pulsewave_channel {
  uint8_t  phase;
  uint16_t counter;
  uint16_t frequency;
} gb_pulsewave_channel_t;

typedef struct gb_apu {
#ifdef __cplusplus
  // I want methods to still be able touch other parts gameboy state like the audio registers.
  gb_apu(gb_state_t &gb_state);

  // Essentially just calls tick m_cycle times (m_cycle/2 times in cgb double speed once that is implemented).
  void spend_mcycles(uint16_t m_cycles);
  // Call once per cycle (1,048,576 Hz regardless of cgb double speed).
  void tick();

private:
  void enable_triggered_channels();
#endif

  gb_pulsewave_channel_t ch1;
  SDL_AudioStream       *output_stream;

#ifdef __cplusplus
  gb_state_t &parent;
#endif
} gb_apu_t;

#ifdef __cplusplus
}
#endif

#endif // GB_APU_H
