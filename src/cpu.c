#include "common.h"
#include <stdint.h>

union inst_param {
  uint8_t r16;
  uint16_t imm16;
};

struct inst {
  enum inst_type {
    // Block 0
    NOP,
    LD_r16_imm16,
  } inst_type;
  union inst_param p1;
  union inst_param p2;
};

struct inst fetch(struct gb_state *gb_state) {
  uint8_t curr_byte = gb_state->mem
  uint8_t block = CRUMB0(gb_state)
}
void execute(struct gb_state *gb_state, struct inst inst) {}
