#ifndef GB_CPU_H
#define GB_CPU_H

#include "common.h"

struct inst_param {
  enum inst_param_type {
    R16,
    IMM16,
  } type;
  union {
    uint8_t r16;
    uint16_t imm16;
  };
};

struct inst {
  enum inst_type {
    // Block 0
    NOP,
    LD,
  } type;
  struct inst_param p1;
  struct inst_param p2;
};

struct inst fetch(struct gb_state *gb_state);
void execute(struct gb_state *gb_state, struct inst inst);

#endif // GB_CPU_H
