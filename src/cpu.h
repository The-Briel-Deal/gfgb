#ifndef GB_CPU_H
#define GB_CPU_H

#include <stdint.h>

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

enum r16_stk {
  R16_STK_BC = 0,
  R16_STK_DE = 1,
  R16_STK_HL = 2,
  R16_STK_AF = 3,
};

enum cond {
  COND_NZ = 0,
  COND_Z = 1,
  COND_NC = 2,
  COND_C = 3,
};

struct inst_param {
  enum inst_param_type {
    R8,
    R16,
    R16_MEM,
    R16_STK,
    IMM16,
    IMM8,
    IMM8_HMEM,
    SP_IMM8, // Stack Pointer + IMM8
    IMM16_MEM,
    B3, // Bit Index
    COND,
    UNKNOWN_INST_BYTE,
    VOID_PARAM_TYPE,
  } type;
  union {
    enum r8 r8;
    enum r16 r16;
    enum r16_mem r16_mem;
    enum r16_stk r16_stk;
    enum cond cond;
    uint8_t imm8;
    uint16_t imm16;
    uint8_t b3;
    uint8_t unknown_inst_byte;
  };
};

struct inst {
  enum inst_type {
    ADC,
    ADD,
    AND,
    BIT,
    CALL,
    CCF,
    CP,
    CPL,
    DAA,
    DEC,
    INC,
    JP,
    JR,
    LD,
    LDH,
    NOP,
    OR,
    POP,
    PUSH,
    RES,
    RET,
    RL,
    RLA,
    RLC,
    RLCA,
    RR,
    RRA,
    RRC,
    RRCA,
    SBC,
    SCF,
    SET,
    SLA,
    SRA,
    SRL,
    SUB,
    SWAP,
    XOR,
    DI,
    EI,

    UNKNOWN_INST,
  } type;
  struct inst_param p1;
  struct inst_param p2;
};

struct gb_state;

uint8_t get_r8(struct gb_state *gb_state, enum r8 r8);
void set_r8(struct gb_state *gb_state, enum r8 r8, uint8_t val);

uint16_t get_r16(struct gb_state *gb_state, enum r16 r16);
void set_r16(struct gb_state *gb_state, enum r16 r16, uint16_t val);

uint16_t get_r16_mem(struct gb_state *gb_state, enum r16_mem r16_mem);
void set_r16_mem(struct gb_state *gb_state, enum r16_mem r16_mem, uint8_t val);

uint16_t get_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk);
void set_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk, uint16_t val);

struct inst fetch(struct gb_state *gb_state);
void execute(struct gb_state *gb_state, struct inst inst);

#endif // GB_CPU_H
