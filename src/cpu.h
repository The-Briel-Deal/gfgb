#ifndef GB_CPU_H
#define GB_CPU_H

#include "common.h"

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

struct inst fetch(struct gb_state *gb_state);
void execute(struct gb_state *gb_state, struct inst inst);

#endif // GB_CPU_H
