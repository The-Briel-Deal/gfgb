#ifndef GB_CPU_H
#define GB_CPU_H

#include "common.h"
#include <stdint.h>
#include <stdio.h>

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

enum r16_mem {
  R16_MEM_BC = 0,
  R16_MEM_DE = 1,
  R16_MEM_HLI = 2,
  R16_MEM_HLD = 3,
};

struct inst_param {
  enum inst_param_type {
    R8,
    R16,
    R16_MEM,
    IMM16,
    IMM8,
    IMM16_MEM,
    UNKNOWN_INST_BYTE,
    VOID_PARAM_TYPE,
  } type;
  union {
    enum r8 r8;
    enum r16 r16;
    enum r16_mem r16_mem;
    uint8_t imm8;
    uint16_t imm16;
    uint8_t unknown_inst_byte;
  };
};

struct inst {
  enum inst_type {
    // Block 0
    NOP,
    LD,
    JP,
    UNKNOWN_INST,
  } type;
  struct inst_param p1;
  struct inst_param p2;
};

void disassemble_rom(FILE *stream, const uint8_t *rom_bytes,
                     const int rom_bytes_len);
struct inst fetch(struct gb_state *gb_state);
void execute(struct gb_state *gb_state, struct inst inst);

#endif // GB_CPU_H
