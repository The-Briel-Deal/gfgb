#ifndef GB_CPU_H
#define GB_CPU_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum r8 {
  R8_B       = 0,
  R8_C       = 1,
  R8_D       = 2,
  R8_E       = 3,
  R8_H       = 4,
  R8_L       = 5,
  R8_HL_DREF = 6,
  R8_A       = 7,
};

enum r16 {
  R16_BC = 0,
  R16_DE = 1,
  R16_HL = 2,
  R16_SP = 3,
};

enum r16_mem {
  R16_MEM_BC  = 0,
  R16_MEM_DE  = 1,
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
  COND_Z  = 1,
  COND_NC = 2,
  COND_C  = 3,
};
enum inst_param_type {
  B3, // Bit Index
  COND,
  IMM16,
  IMM16_MEM,
  IMM8,
  E8,
  IMM8_HMEM,
  R16,
  R16_MEM,
  R16_STK,
  R8,
  TGT3,    // "rst's target address, divided by 8"
  SP_IMM8, // Stack Pointer + IMM8
  UNKNOWN_INST_BYTE,
  VOID_PARAM_TYPE,
};
typedef enum inst_param_type inst_param_type_t;

typedef uint8_t              r8_t;
typedef uint8_t              r16_t;
typedef uint8_t              r16_mem_t;
typedef uint8_t              r16_stk_t;
typedef uint8_t              cond_t;
struct inst_param {
  inst_param_type_t type;
  union {
    r8_t      r8;
    r16_t     r16;
    r16_mem_t r16_mem;
    r16_stk_t r16_stk;
    cond_t    cond;
    uint8_t   imm8;
    uint16_t  imm16;
    uint8_t   b3;
    uint8_t   tgt3;
    uint8_t   unknown_inst_byte;
    uint8_t   void_val; // I need to put something in the union for cpp compilers to not complain, so I just put 0 in
                        // here for VOID_PARAM's.
  };
};
typedef struct inst_param inst_param_t;

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
  DI,
  EI,
  HALT,
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
  RETI,
  RL,
  RLA,
  RLC,
  RLCA,
  RR,
  RRA,
  RRC,
  RRCA,
  RST,
  SBC,
  SCF,
  SET,
  SLA,
  SRA,
  SRL,
  STOP,
  SUB,
  SWAP,
  XOR,

  UNKNOWN_INST,
};
typedef enum inst_type inst_type_t;

struct inst {
  inst_type_t  type;
  inst_param_t p1;
  inst_param_t p2;
};
typedef struct inst inst_t;

struct gb_state;

uint8_t     get_r8(struct gb_state *gb_state, enum r8 r8);
void        set_r8(struct gb_state *gb_state, enum r8 r8, uint8_t val);

uint16_t    get_pc(struct gb_state *gb_state);
void        set_pc(struct gb_state *gb_state, uint16_t new_pc);

uint16_t    get_r16(struct gb_state *gb_state, enum r16 r16);
void        set_r16(struct gb_state *gb_state, enum r16 r16, uint16_t val);

uint16_t    get_r16_mem(struct gb_state *gb_state, enum r16_mem r16_mem);
void        set_r16_mem(struct gb_state *gb_state, enum r16_mem r16_mem, uint8_t val);

uint16_t    get_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk);
void        set_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk, uint16_t val);

void        set_ime(struct gb_state *gb_state, bool on);
bool        get_ime(struct gb_state *gb_state); // This is only for tests and debugging.

struct inst fetch(struct gb_state *gb_state);
void        execute(struct gb_state *gb_state, struct inst inst);

void        handle_interrupts(struct gb_state *gb_state);

#ifdef __cplusplus
}
#endif

#endif // GB_CPU_H
