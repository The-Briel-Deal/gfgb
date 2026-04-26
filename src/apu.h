#ifndef GB_APU_H
#define GB_APU_H

#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;
typedef struct gb_state gb_state_t;

typedef struct gb_apu {
#ifdef __cplusplus
  // I want methods to still be able touch other parts gameboy state like the audio registers.
  gb_apu(gb_state_t &gb_state);
  void update();

  gb_state_t &parent;
#endif
} gb_apu_t;

#ifdef __cplusplus
}
#endif

#endif // GB_APU_H
