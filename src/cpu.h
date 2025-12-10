#ifndef GB_CPU_H
#define GB_CPU_H

#include "common.h"

enum r8 {
  R8_B = 0,
  R8_C = 1,
  R8_D = 2,
  R8_E = 3,
  R8_H = 4,
  R8_L = 5,
  R8_HL_DREF = 6,
  R8_A = 7,
};

enum r16 {
  R16_BC = 0,
  R16_DE = 1,
  R16_HL = 2,
  R16_SP = 3,
};

struct inst_param {
  enum inst_param_type {
    R8,
    R16,
    R16_MEM,
    IMM16,
    IMM16_MEM,
  } type;
  union {
    enum r8 r8;
    enum r16 r16;
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
