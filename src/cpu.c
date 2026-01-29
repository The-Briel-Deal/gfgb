#include "cpu.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

enum flags : uint8_t {
  FLAG_Z = (1 << 7),
  FLAG_N = (1 << 6),
  FLAG_H = (1 << 5),
  FLAG_C = (1 << 4),
};

#define R8_PARAM(r)                                                                                                    \
  (struct inst_param) { .type = R8, .r8 = r }
#define R16_PARAM(r)                                                                                                   \
  (struct inst_param) { .type = R16, .r16 = r }
#define R16_MEM_PARAM(r)                                                                                               \
  (struct inst_param) { .type = R16_MEM, .r16_mem = r }
#define R16_STK_PARAM(r)                                                                                               \
  (struct inst_param) { .type = R16_STK, .r16_stk = r }
#define IMM16_PARAM(imm)                                                                                               \
  (struct inst_param) { .type = IMM16, .imm16 = imm }
#define SP_IMM8_PARAM(imm)                                                                                             \
  (struct inst_param) { .type = SP_IMM8, .imm8 = imm }
#define IMM8_PARAM(imm)                                                                                                \
  (struct inst_param) { .type = IMM8, .imm8 = imm }
#define E8_PARAM(imm)                                                                                                  \
  (struct inst_param) { .type = E8, .imm8 = imm }
#define IMM8_HMEM_PARAM(imm)                                                                                           \
  (struct inst_param) { .type = IMM8_HMEM, .imm8 = imm }
#define IMM16_MEM_PARAM(imm)                                                                                           \
  (struct inst_param) { .type = IMM16_MEM, .imm16 = imm }
#define B3_PARAM(b)                                                                                                    \
  (struct inst_param) { .type = B3, .b3 = b }
#define TGT3_PARAM(b)                                                                                                  \
  (struct inst_param) { .type = TGT3, .tgt3 = b }
#define COND_PARAM(cond)                                                                                               \
  (struct inst_param) { .type = COND, .r8 = cond }
#define UNKNOWN_INST_BYTE_PARAM(b)                                                                                     \
  (struct inst_param) { .type = UNKNOWN_INST_BYTE, .unknown_inst_byte = b }
#define VOID_PARAM                                                                                                     \
  (struct inst_param) { .type = VOID_PARAM_TYPE }

#define SPEND_MCYCLES(n) gb_state->m_cycles_elapsed += n

static inline uint8_t next8(struct gb_state *gb_state) {
  uint8_t val = read_mem8(gb_state, gb_state->regs.pc);
  gb_state->regs.pc += 1;
  // unmap the bootrom once the PC makes it passed the end
  if (gb_state->bootrom_mapped && gb_state->regs.pc >= 0x0100) {
    gb_state->bootrom_mapped = false;
  }
  return val;
}

static inline uint16_t next16(struct gb_state *gb_state) {
  uint16_t val = read_mem16(gb_state, gb_state->regs.pc);
  gb_state->regs.pc += 2;
  return val;
}

uint8_t get_r8(struct gb_state *gb_state, enum r8 r8) {
  struct regs *r = &gb_state->regs;
  switch (r8) {
  case R8_B: return r->b;
  case R8_C: return r->c;
  case R8_D: return r->d;
  case R8_E: return r->e;
  case R8_H: return r->h;
  case R8_L: return r->l;
  case R8_HL_DREF: return read_mem8(gb_state, COMBINED_REG((*r), h, l));
  case R8_A: return r->a;
  default: abort();
  }
}

void set_r8(struct gb_state *gb_state, enum r8 r8, uint8_t val) {
  struct regs *r = &gb_state->regs;
  switch (r8) {
  case R8_B: r->b = val; return;
  case R8_C: r->c = val; return;
  case R8_D: r->d = val; return;
  case R8_E: r->e = val; return;
  case R8_H: r->h = val; return;
  case R8_L: r->l = val; return;
  case R8_HL_DREF: write_mem8(gb_state, COMBINED_REG((*r), h, l), val); return;
  case R8_A: r->a = val; return;
  default: abort();
  }
}

uint16_t get_pc(struct gb_state *gb_state) { return gb_state->regs.pc; }
void set_pc(struct gb_state *gb_state, uint16_t new_pc) { gb_state->regs.pc = new_pc; }

uint16_t get_r16(struct gb_state *gb_state, enum r16 r16) {
  struct regs *r = &gb_state->regs;
  switch (r16) {
  case R16_BC: return COMBINED_REG((*r), b, c);
  case R16_DE: return COMBINED_REG((*r), d, e);
  case R16_HL: return COMBINED_REG((*r), h, l);
  case R16_SP: return r->sp;
  default: abort(); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

void set_r16(struct gb_state *gb_state, enum r16 r16, uint16_t val) {
  struct regs *r = &gb_state->regs;
  switch (r16) {
  case R16_BC: SET_COMBINED_REG((*r), b, c, val); return;
  case R16_DE: SET_COMBINED_REG((*r), d, e, val); return;
  case R16_HL: SET_COMBINED_REG((*r), h, l, val); return;
  case R16_SP: r->sp = val; return;
  default: abort(); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

void set_r16_mem(struct gb_state *gb_state, enum r16_mem r16_mem, uint8_t val) {
  struct regs *r = &gb_state->regs;
  uint16_t mem_offset;
  switch (r16_mem) {
  case R16_MEM_BC: mem_offset = COMBINED_REG((*r), b, c); break;
  case R16_MEM_DE: mem_offset = COMBINED_REG((*r), d, e); break;
  case R16_MEM_HLI:
    mem_offset = COMBINED_REG((*r), h, l);
    set_r16(gb_state, R16_HL, mem_offset + 1);
    break;
  case R16_MEM_HLD:
    mem_offset = COMBINED_REG((*r), h, l);
    set_r16(gb_state, R16_HL, mem_offset - 1);
    break;
  default: exit(1); // bc, de, hl, and sp are the only valid r16 registers.
  }
  write_mem8(gb_state, mem_offset, val);
}

uint16_t get_r16_mem(struct gb_state *gb_state, enum r16_mem r16_mem) {
  assert(r16_mem <= R16_MEM_HLD);
  uint16_t addr;
  switch (r16_mem) {
  case R16_MEM_BC: return get_r16(gb_state, R16_BC);
  case R16_MEM_DE: return get_r16(gb_state, R16_DE);
  case R16_MEM_HLI: // Increment HL after deref
    addr = get_r16(gb_state, R16_HL);
    set_r16(gb_state, R16_HL, addr + 1);
    return addr;
  case R16_MEM_HLD: // Decrement HL after deref
    addr = get_r16(gb_state, R16_HL);
    set_r16(gb_state, R16_HL, addr - 1);
    return addr;
  }
  abort(); // This should never happen unless something is very wrong.
}
uint16_t get_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk) {
  assert(r16_stk <= R16_STK_AF);
  switch (r16_stk) {
  case R16_STK_BC: return get_r16(gb_state, R16_BC);
  case R16_STK_DE: return get_r16(gb_state, R16_DE);
  case R16_STK_HL: return get_r16(gb_state, R16_HL);
  case R16_STK_AF: return COMBINED_REG(gb_state->regs, a, f);
  }
  abort(); // This should never happen unless something is very wrong.
}
void set_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk, uint16_t val) {
  struct regs *r = &gb_state->regs;
  switch (r16_stk) {
  case R16_STK_BC: SET_COMBINED_REG((*r), b, c, val); return;
  case R16_STK_DE: SET_COMBINED_REG((*r), d, e, val); return;
  case R16_STK_HL: SET_COMBINED_REG((*r), h, l, val); return;
  case R16_STK_AF: SET_COMBINED_REG((*r), a, f, val & 0xFFF0); return; // only set upper 4 on flag reg
  default: abort(); // bc, de, hl, and af are the only valid r16_stk registers.
  }
}

void set_ime(struct gb_state *gb_state, bool on) { gb_state->regs.io.ime = on; }
bool get_ime(struct gb_state *gb_state) { return gb_state->regs.io.ime; } // This is only for tests and debugging.

void set_flags(struct gb_state *gb_state, enum flags flags, bool on) {
  if (on) {
    gb_state->regs.f |= flags;
  } else {
    gb_state->regs.f &= ~flags;
  }
}

#define CONDITION_CODE_MASK 0b00011000
#define ARITHMETIC_R8_MASK  0b00000111
#define ARITHMETIC_OP_MASK  0b00111000

struct inst fetch(struct gb_state *gb_state) {
  uint8_t curr_byte = next8(gb_state);
  uint8_t block = CRUMB0(curr_byte);
  switch (block) {
  case /* block */ 0:
    // First we'll look for all the instructions where params aren't in the first byte. This way we can specify them in
    // hex for readability.
    switch (curr_byte) {
    case 0x00: return (struct inst){.type = NOP, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    case 0x10:
      // TODO: apparently STOP has some weird edge cases where it can consume two bytes.
      // See: https://gist.github.com/SonoSooS/c0055300670d678b5ae8433e20bea595#nop-and-stop
      return (struct inst){.type = STOP, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    switch (NIBBLE1(curr_byte)) {
    case 0b0001:
      return (struct inst){
          .type = LD,
          .p1 = R16_PARAM(CRUMB1(curr_byte)),
          .p2 = IMM16_PARAM(next16(gb_state)),
      };
    case 0b0010:
      return (struct inst){
          .type = LD,
          .p1 = R16_MEM_PARAM(CRUMB1(curr_byte)),
          .p2 = R8_PARAM(R8_A),
      };
    case 0b0011:
      return (struct inst){
          .type = INC,
          .p1 = R16_PARAM(CRUMB1(curr_byte)),
          .p2 = VOID_PARAM,
      };
    case 0b1011:
      return (struct inst){
          .type = DEC,
          .p1 = R16_PARAM(CRUMB1(curr_byte)),
          .p2 = VOID_PARAM,
      };
    case 0b1001:
      return (struct inst){
          .type = ADD,
          .p1 = R16_PARAM(R16_HL),
          .p2 = R16_PARAM(CRUMB1(curr_byte)),
      };
    case 0b1010:
      return (struct inst){
          .type = LD,
          .p1 = R8_PARAM(R8_A),
          .p2 = R16_MEM_PARAM(CRUMB1(curr_byte)),
      };
    case 0b1000:
      if (CRUMB1(curr_byte) == 0b00)
        return (struct inst){
            .type = LD,
            .p1 = IMM16_MEM_PARAM(next16(gb_state)),
            .p2 = R16_PARAM(R16_SP),
        };
      break;
    }
    // ld r8, imm8
    if ((curr_byte & 0b00000111) == 0b00000110)
      return (struct inst){
          .type = LD, .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3), .p2 = IMM8_PARAM(next8(gb_state))};
    // inc r8
    if ((curr_byte & 0b00000111) == 0b00000100)
      return (struct inst){.type = INC, .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3), .p2 = VOID_PARAM};
    // dec r8
    if ((curr_byte & 0b00000111) == 0b00000101)
      return (struct inst){.type = DEC, .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3), .p2 = VOID_PARAM};

    // jr imm8
    if (curr_byte == 0b00011000) return (struct inst){.type = JR, .p1 = IMM8_PARAM(next8(gb_state)), .p2 = VOID_PARAM};
    // jr cond, imm8
    if ((curr_byte & ~CONDITION_CODE_MASK) == 0b00100000)
      return (struct inst){
          .type = JR, .p1 = COND_PARAM((curr_byte & CONDITION_CODE_MASK) >> 3), .p2 = IMM8_PARAM(next8(gb_state))};

    // rlca
    if (curr_byte == 0b00000111) {
      return (struct inst){.type = RLCA, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // rrca
    if (curr_byte == 0b00001111) {
      return (struct inst){.type = RRCA, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // rla
    if (curr_byte == 0b00010111) {
      return (struct inst){.type = RLA, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // rra
    if (curr_byte == 0b00011111) {
      return (struct inst){.type = RRA, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // daa
    if (curr_byte == 0b00100111) {
      return (struct inst){.type = DAA, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // cpl
    if (curr_byte == 0b00101111) {
      return (struct inst){.type = CPL, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // scf
    if (curr_byte == 0b00110111) {
      return (struct inst){.type = SCF, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    // ccf
    if (curr_byte == 0b00111111) {
      return (struct inst){.type = CCF, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    }
    break;
  case /* block */ 1:
    if (curr_byte == 0x76) return (struct inst){.type = HALT, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    return (struct inst){
        .type = LD, .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3), .p2 = R8_PARAM((curr_byte & 0b00000111) >> 0)};

  case /* block */ 2: {
    struct inst inst = {
        .type = UNKNOWN_INST, .p1 = R8_PARAM(R8_A), .p2 = R8_PARAM((curr_byte & ARITHMETIC_R8_MASK) >> 0)};
    uint8_t arithmetic_op_code = (curr_byte & ARITHMETIC_OP_MASK) >> 3;
    switch (arithmetic_op_code) {
    case 0: inst.type = ADD; break;
    case 1: inst.type = ADC; break;
    case 2: inst.type = SUB; break;
    case 3: inst.type = SBC; break;
    case 4: inst.type = AND; break;
    case 5: inst.type = XOR; break;
    case 6: inst.type = OR; break;
    case 7: inst.type = CP; break;
    }
    if (inst.type != UNKNOWN_INST) return inst;
    break;
  }
  case /* block */ 3:
    switch (curr_byte) {
    case 0xC9: return (struct inst){.type = RET, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    case 0xD9: return (struct inst){.type = RETI, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    case 0xE0: return (struct inst){.type = LDH, .p1 = IMM8_HMEM_PARAM(next8(gb_state)), .p2 = R8_PARAM(R8_A)};
    case 0xE2: return (struct inst){.type = LDH, .p1 = R8_PARAM(R8_C), .p2 = R8_PARAM(R8_A)};
    case 0xE8: return (struct inst){.type = ADD, .p1 = R16_PARAM(R16_SP), .p2 = E8_PARAM(next8(gb_state))};
    case 0xE9: return (struct inst){.type = JP, .p1 = R16_PARAM(R16_HL), .p2 = VOID_PARAM};
    case 0xEA: return (struct inst){.type = LD, .p1 = IMM16_MEM_PARAM(next16(gb_state)), .p2 = R8_PARAM(R8_A)};
    case 0xF0: return (struct inst){.type = LDH, .p1 = R8_PARAM(R8_A), .p2 = IMM8_HMEM_PARAM(next8(gb_state))};
    case 0xF2: return (struct inst){.type = LDH, .p1 = R8_PARAM(R8_A), .p2 = R8_PARAM(R8_C)};
    case 0xF8: return (struct inst){.type = LD, .p1 = R16_PARAM(R16_HL), .p2 = SP_IMM8_PARAM(next8(gb_state))};
    case 0xF9: return (struct inst){.type = LD, .p1 = R16_PARAM(R16_SP), .p2 = R16_PARAM(R16_HL)};
    case 0xFA: return (struct inst){.type = LD, .p1 = R8_PARAM(R8_A), .p2 = IMM16_MEM_PARAM(next16(gb_state))};
    }
    if (NIBBLE1(curr_byte) == 0b0001) // Pop r16stk
      return (struct inst){.type = POP, .p1 = R16_STK_PARAM(CRUMB1(curr_byte)), .p2 = VOID_PARAM};
    if (NIBBLE1(curr_byte) == 0b0101) // Push r16stk
      return (struct inst){.type = PUSH, .p1 = R16_STK_PARAM(CRUMB1(curr_byte)), .p2 = VOID_PARAM};
    if (curr_byte == 0b11000011) // Unconditional jump
      return (struct inst){.type = JP, .p1 = IMM16_PARAM(next16(gb_state)), .p2 = VOID_PARAM};
    if ((curr_byte & ~CONDITION_CODE_MASK) == 0b11000000) // RET
      return (struct inst){.type = RET, .p1 = COND_PARAM((curr_byte & CONDITION_CODE_MASK) >> 3), .p2 = VOID_PARAM};

    if (curr_byte == 0b11001101) // Unconditional call
      return (struct inst){.type = CALL, .p1 = IMM16_PARAM(next16(gb_state)), .p2 = VOID_PARAM};

    if ((curr_byte & ~CONDITION_CODE_MASK) == 0b11000100) // Conditional call
      return (struct inst){
          .type = CALL, .p1 = COND_PARAM((curr_byte & CONDITION_CODE_MASK) >> 3), .p2 = IMM16_PARAM(next16(gb_state))};

    if ((curr_byte & ~0b00111000) == 0b11000111)
      return (struct inst){.type = RST, .p1 = TGT3_PARAM((0b00111000 & curr_byte) >> 3), .p2 = VOID_PARAM};

    // Control Instructions
    if (curr_byte == 0xF3) // DI (Disable Interrupts)
      return (struct inst){.type = DI, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
    if (curr_byte == 0xFB) // EI (Enable Interrupts)
      return (struct inst){.type = EI, .p1 = VOID_PARAM, .p2 = VOID_PARAM};

    if (OCTAL2(curr_byte) == 0b110) {
      switch (OCTAL1(curr_byte)) {
      case 0: return (struct inst){.type = ADD, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 1: return (struct inst){.type = ADC, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 2: return (struct inst){.type = SUB, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 3: return (struct inst){.type = SBC, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 4: return (struct inst){.type = AND, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 5: return (struct inst){.type = XOR, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 6: return (struct inst){.type = OR, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      case 7: return (struct inst){.type = CP, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};
      }
    }

    if ((curr_byte & ~CONDITION_CODE_MASK) == 0b11000010) // JP COND, IMM16
      return (struct inst){
          .type = JP, .p1 = COND_PARAM((curr_byte & CONDITION_CODE_MASK) >> 3), .p2 = IMM16_PARAM(next16(gb_state))};

    // 0xCB prefix instructions
    if (curr_byte == 0xCB) {
      uint8_t cb_suffix = next8(gb_state);
      struct inst inst = {.type = UNKNOWN_INST, .p1 = VOID_PARAM, .p2 = VOID_PARAM};

      switch (CRUMB0(cb_suffix)) {
      case 0b00:
        switch (OCTAL1(cb_suffix)) {
        case 0b000: // RLC R8
          inst.type = RLC;
          goto r8_inst;
        case 0b001: // RRC R8
          inst.type = RRC;
          goto r8_inst;
        case 0b010: // RL R8
          inst.type = RL;
          goto r8_inst;
        case 0b011: // RR R8
          inst.type = RR;
          goto r8_inst;
        case 0b100: // SLA R8
          inst.type = SLA;
          goto r8_inst;
        case 0b101: // SRA R8
          inst.type = SRA;
          goto r8_inst;
        case 0b110: // SWAP R8
          inst.type = SWAP;
          goto r8_inst;
        case 0b111: // SRL R8
          inst.type = SRL;
          goto r8_inst;
        }
        break;
      case 0b01: // BIT B3, R8
        inst.type = BIT;
        goto b3_r8_inst;
      case 0b10: // RES B3, R8
        inst.type = RES;
        goto b3_r8_inst;
      case 0b11: // SET B3, R8
        inst.type = SET;
        goto b3_r8_inst;
      }
      // every possible 0xCB suffix maps to an opcode.
      unreachable();
    b3_r8_inst:
      inst.p1 = B3_PARAM(OCTAL1(cb_suffix));
      inst.p2 = R8_PARAM(OCTAL2(cb_suffix));
      return inst;

    r8_inst:
      inst.p1 = R8_PARAM(OCTAL2(cb_suffix));
      return inst;
    }

    break;
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown instruction 0x%.2X.", curr_byte);
  return (struct inst){.type = UNKNOWN_INST, .p1 = UNKNOWN_INST_BYTE_PARAM(curr_byte), .p2 = VOID_PARAM};
}

#define IS_R16(param)       (param.type == R16)
#define IS_R16_MEM(param)   (param.type == R16_MEM)
#define IS_R8(param)        (param.type == R8)
#define IS_E8(param)        (param.type == E8)
#define IS_IMM16(param)     (param.type == IMM16)
#define IS_IMM16_MEM(param) (param.type == IMM16_MEM)
#define IS_IMM8(param)      (param.type == IMM8)
#define IS_SP_IMM8(param)   (param.type == SP_IMM8)
#define IS_TGT3(param)      (param.type == TGT3)
#define IS_COND(param)      (param.type == COND)
#define IS_VOID(param)      (param.type == VOID_PARAM_TYPE)

static void ex_ld(struct gb_state *gb_state, struct inst inst) {
  struct inst_param dest = inst.p1;
  struct inst_param src = inst.p2;
  if (IS_R16(dest) && IS_IMM16(src)) {
    SPEND_MCYCLES(3);
    set_r16(gb_state, dest.r16, src.imm16);
    return;
  }
  if (IS_R16_MEM(dest) && IS_R8(src)) {
    SPEND_MCYCLES(2);
    write_mem8(gb_state, get_r16_mem(gb_state, dest.r16_mem), get_r8(gb_state, src.r8));
    return;
  }
  if (IS_R8(dest)) {
    uint8_t src_val;
    if (dest.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    if (IS_R16_MEM(src)) {
      SPEND_MCYCLES(2);
      src_val = read_mem8(gb_state, get_r16_mem(gb_state, src.r16_mem));
    } else if (IS_IMM16_MEM(src)) {
      SPEND_MCYCLES(4);
      src_val = read_mem8(gb_state, src.imm16);
    } else if (IS_IMM8(src)) {
      SPEND_MCYCLES(2);
      src_val = src.imm8;
    } else if (IS_R8(src)) {
      SPEND_MCYCLES(1);
      if (src.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
      src_val = get_r8(gb_state, src.r8);
    } else
      goto not_implemented;
    set_r8(gb_state, dest.r8, src_val);
    return;
  }
  if (IS_IMM16_MEM(dest) && IS_R16(src)) {
    SPEND_MCYCLES(5);
    write_mem16(gb_state, dest.imm16, get_r16(gb_state, src.r16));
    return;
  }
  if (IS_IMM16_MEM(dest) && IS_R8(src)) {
    SPEND_MCYCLES(4);
    write_mem8(gb_state, dest.imm16, get_r8(gb_state, src.r8));
    return;
  }
  if (IS_R16(dest) && IS_SP_IMM8(src)) {
    assert(dest.r16 == R16_HL); // this inst should always be setting HL
    SPEND_MCYCLES(3);
    uint16_t sp_val = get_r16(gb_state, R16_SP);
    int8_t add = *(int8_t *)&src.imm8;
    uint16_t result = sp_val;
    result += add;
    set_r16(gb_state, dest.r16, result);
    set_flags(gb_state, FLAG_Z | FLAG_N, false);
    set_flags(gb_state, FLAG_H, ((sp_val ^ add ^ result) & 0x10) == 0x10);
    set_flags(gb_state, FLAG_C, ((sp_val ^ add ^ result) & 0x100) == 0x100);
    return;
  }

  if (IS_R16(dest) && IS_R16(src)) {
    assert(dest.r16 == R16_SP);
    assert(src.r16 == R16_HL);
    SPEND_MCYCLES(2);
    uint16_t hl_val = get_r16(gb_state, R16_HL);
    set_r16(gb_state, R16_SP, hl_val);
    return;
  }
not_implemented:
  NOT_IMPLEMENTED("Unknown load instruction");
}
static void ex_ldh(struct gb_state *gb_state, struct inst inst) {
  struct inst_param dest = inst.p1;
  struct inst_param src = inst.p2;
  struct regs *r = &gb_state->regs;
  if (src.type == R8 && src.r8 == R8_A) {
    if (dest.type == IMM8_HMEM) {
      SPEND_MCYCLES(3);
      write_mem8(gb_state, 0xFF00 + dest.imm8, r->a);
      return;
    }
    if (dest.type == R8 && dest.r8 == R8_C) {
      SPEND_MCYCLES(2);
      write_mem8(gb_state, 0xFF00 + r->c, r->a);
      return;
    }
    unreachable();
  }
  if (dest.type == R8 && dest.r8 == R8_A) {
    if (src.type == IMM8_HMEM) {
      SPEND_MCYCLES(3);
      set_r8(gb_state, R8_A, read_mem8(gb_state, 0xFF00 + src.imm8));
      return;
    }
    if (src.type == R8 && src.r8 == R8_C) {
      SPEND_MCYCLES(2);
      set_r8(gb_state, R8_A, read_mem8(gb_state, 0xFF00 + r->c));
      return;
    }
    unreachable();
  }
  unreachable();
}
static void ex_nop(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == NOP);
  SPEND_MCYCLES(1);
}
static void ex_stop(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == STOP);
  // For some reason the SST tests expect stop to take 3 M-Cycles, this contradicts other documentation, but i'll just
  // go with it until it causes problems.
  SPEND_MCYCLES(3);
}
static void push16(struct gb_state *gb_state, uint16_t val) {
  // little endian
  write_mem8(gb_state, --gb_state->regs.sp, (val & 0xFF00) >> 8);
  write_mem8(gb_state, --gb_state->regs.sp, (val & 0x00FF) >> 0);
}
static void ex_push(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == PUSH);
  assert(inst.p1.type == R16_STK);
  assert(inst.p2.type == VOID_PARAM_TYPE);
  SPEND_MCYCLES(4);
  push16(gb_state, get_r16_stk(gb_state, inst.p1.r16_stk));
}

static uint16_t pop16(struct gb_state *gb_state) {
  uint16_t val = 0;

  // little endian
  val |= read_mem8(gb_state, gb_state->regs.sp++) << 0;
  val |= read_mem8(gb_state, gb_state->regs.sp++) << 8;
  return val;
}
static void ex_pop(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == POP);
  assert(inst.p1.type == R16_STK);
  assert(inst.p2.type == VOID_PARAM_TYPE);
  SPEND_MCYCLES(3);
  set_r16_stk(gb_state, inst.p1.r16_stk, pop16(gb_state));
}

static void ex_inc(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == INC);
  assert(IS_R8(inst.p1) || IS_R16(inst.p1));
  assert(IS_VOID(inst.p2));

  if (IS_R8(inst.p1)) {
    SPEND_MCYCLES(1);
    if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
    uint8_t val;
    val = get_r8(gb_state, inst.p1.r8);
    set_r8(gb_state, inst.p1.r8, val + 1);
    set_flags(gb_state, FLAG_Z, (uint8_t)(val + 1) == 0x00);
    set_flags(gb_state, FLAG_N, 0);
    set_flags(gb_state, FLAG_H, ((val & 0x0F) + 1) == 0x10);
    return;
  }
  if (IS_R16(inst.p1)) {
    SPEND_MCYCLES(2);
    uint16_t val;
    val = get_r16(gb_state, inst.p1.r16);
    set_r16(gb_state, inst.p1.r16, val + 1);
    // no flags affected for inc r16
    return;
  }
  abort();
}

static void ex_adc(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == ADC);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t carry = (gb_state->regs.f & FLAG_C) >> 4;
  uint8_t add;
  if (IS_R8(inst.p2)) {
    SPEND_MCYCLES(1);
    if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    add = get_r8(gb_state, inst.p2.r8);
  } else {
    assert(IS_IMM8(inst.p2));
    SPEND_MCYCLES(2);
    add = inst.p2.imm8;
  }
  uint8_t result = (uint8_t)val + add + carry;
  set_r8(gb_state, R8_A, result);
  set_flags(gb_state, FLAG_Z, result == 0);
  set_flags(gb_state, FLAG_N, false);
  set_flags(gb_state, FLAG_H, ((val & 0x0F) + (add & 0x0F) + (carry & 0x0F)) > 0x0F);
  set_flags(gb_state, FLAG_C, ((val & 0xFF) + (add & 0xFF) + (carry & 0xFF)) > 0xFF);
}
static void ex_sbc(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SBC);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t carry = (gb_state->regs.f & FLAG_C) >> 4;
  uint8_t sub;
  if (IS_R8(inst.p2)) {
    SPEND_MCYCLES(1);
    if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    sub = get_r8(gb_state, inst.p2.r8);
  } else {
    assert(IS_IMM8(inst.p2));
    SPEND_MCYCLES(2);
    sub = inst.p2.imm8;
  }
  uint8_t result = (uint8_t)val - sub - carry;
  set_r8(gb_state, R8_A, result);
  set_flags(gb_state, FLAG_Z, result == 0);
  set_flags(gb_state, FLAG_N, true);
  set_flags(gb_state, FLAG_H, ((val & 0x0F) - (sub & 0x0F) - (carry & 0x0F)) < 0x00);
  set_flags(gb_state, FLAG_C, ((val & 0xFF) - (sub & 0xFF) - (carry & 0xFF)) < 0x00);
}
static void ex_add(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == ADD);
  assert((IS_R8(inst.p1) && inst.p1.r8 == R8_A) ||
         (IS_R16(inst.p1) && (inst.p1.r16 == R16_HL || inst.p1.r16 == R16_SP)));
  assert(IS_R8(inst.p2) || IS_IMM8(inst.p2) || IS_R16(inst.p2) || IS_E8(inst.p2));

  if (IS_R8(inst.p1) && inst.p1.r8 == R8_A) {
    uint8_t a_val = get_r8(gb_state, R8_A);
    uint8_t p2_val;
    if (IS_R8(inst.p2)) {
      SPEND_MCYCLES(1);
      if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
      p2_val = get_r8(gb_state, inst.p2.r8);
    } else {
      assert(IS_IMM8(inst.p2));
      SPEND_MCYCLES(2);
      p2_val = inst.p2.imm8;
    }
    set_r8(gb_state, inst.p1.r8, a_val + p2_val);
    set_flags(gb_state, FLAG_Z, (uint8_t)(a_val + p2_val) == 0x00);
    set_flags(gb_state, FLAG_N, 0);
    set_flags(gb_state, FLAG_H, ((a_val & 0x0F) + (p2_val & 0x0F)) >= 0x10);
    set_flags(gb_state, FLAG_C, (a_val + p2_val) >= 0x100);
    return;
  }
  if (IS_R16(inst.p1) && (inst.p1.r16 == R16_HL)) {
    assert(IS_R16(inst.p2));
    SPEND_MCYCLES(2);
    uint16_t hl_val = get_r16(gb_state, R16_HL);
    uint16_t p2_val = get_r16(gb_state, inst.p2.r16);
    set_r16(gb_state, R16_HL, hl_val + p2_val);
    set_flags(gb_state, FLAG_N, 0);
    set_flags(gb_state, FLAG_H, ((hl_val & 0x0FFF) + (p2_val & 0x0FFF)) >= 0x1000);
    set_flags(gb_state, FLAG_C, (hl_val + p2_val) >= 0x10000);
    return;
  }

  if (IS_R16(inst.p1) && (inst.p1.r16 == R16_SP)) {
    assert(IS_E8(inst.p2));
    SPEND_MCYCLES(4);
    uint16_t sp_val = get_r16(gb_state, R16_SP);
    int8_t p2_val = *(int8_t *)&inst.p2.imm8;
    uint16_t result = sp_val + p2_val;
    set_r16(gb_state, R16_SP, result);
    set_flags(gb_state, FLAG_N | FLAG_Z, false);
    // TODO: I'm not positive these are correct since p2_val is signed. I need to double check this against other
    // emulators.
    set_flags(gb_state, FLAG_H, ((sp_val ^ p2_val ^ result) & 0x10) == 0x10);
    set_flags(gb_state, FLAG_C, ((sp_val ^ p2_val ^ result) & 0x100) == 0x100);
    return;
  }
  unreachable();
}
static void ex_sub(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SUB);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  assert(IS_R8(inst.p2) || IS_IMM8(inst.p2));

  if (IS_R8(inst.p1) && inst.p1.r8 == R8_A) {
    uint8_t a_val = get_r8(gb_state, R8_A);
    uint8_t p2_val;
    if (IS_R8(inst.p2)) {
      SPEND_MCYCLES(1);
      if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
      p2_val = get_r8(gb_state, inst.p2.r8);
    } else {
      assert(IS_IMM8(inst.p2));
      SPEND_MCYCLES(2);
      p2_val = inst.p2.imm8;
    }
    uint8_t result = a_val - p2_val;
    set_r8(gb_state, inst.p1.r8, result);
    set_flags(gb_state, FLAG_Z, (uint8_t)(result) == 0x00);
    set_flags(gb_state, FLAG_N, 1);
    set_flags(gb_state, FLAG_H, (a_val & 0x0F) < (p2_val & 0x0F));
    set_flags(gb_state, FLAG_C, a_val < p2_val);
    return;
  }
  unreachable();
}

static void ex_or(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == OR);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  assert(IS_R8(inst.p2) || IS_IMM8(inst.p2));
  struct regs *r = &gb_state->regs;
  uint8_t p2_val;
  if (IS_R8(inst.p2)) {
    SPEND_MCYCLES(1);
    if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    p2_val = get_r8(gb_state, inst.p2.r8);
  } else if (IS_IMM8(inst.p2)) {
    SPEND_MCYCLES(2);
    p2_val = inst.p2.imm8;
  } else {
    unreachable();
  }
  r->a |= p2_val;
  set_flags(gb_state, FLAG_H | FLAG_N | FLAG_C, false);
  set_flags(gb_state, FLAG_Z, r->a == 0);
}

static void ex_and(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == AND);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  assert(IS_R8(inst.p2) || IS_IMM8(inst.p2));
  struct regs *r = &gb_state->regs;
  uint8_t p2_val;
  if (IS_R8(inst.p2)) {
    SPEND_MCYCLES(1);
    if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    p2_val = get_r8(gb_state, inst.p2.r8);
  } else if (IS_IMM8(inst.p2)) {
    SPEND_MCYCLES(2);
    p2_val = inst.p2.imm8;
  } else {
    unreachable();
  }
  r->a &= p2_val;
  set_flags(gb_state, FLAG_N | FLAG_C, false);
  set_flags(gb_state, FLAG_H, true);
  set_flags(gb_state, FLAG_Z, r->a == 0);
}

static void ex_xor(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == XOR);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  assert(IS_R8(inst.p2) || IS_IMM8(inst.p2));
  struct regs *r = &gb_state->regs;
  uint8_t p2_val;
  if (IS_R8(inst.p2)) {
    SPEND_MCYCLES(1);
    if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    p2_val = get_r8(gb_state, inst.p2.r8);
  } else if (IS_IMM8(inst.p2)) {
    SPEND_MCYCLES(2);
    p2_val = inst.p2.imm8;
  } else {
    unreachable();
  }
  r->a ^= p2_val;
  set_flags(gb_state, FLAG_H | FLAG_N | FLAG_C, false);
  set_flags(gb_state, FLAG_Z, r->a == 0);
}

static void ex_bit(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == BIT);
  assert(inst.p1.type == B3);
  assert(IS_R8(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
  uint8_t val = get_r8(gb_state, inst.p2.r8);
  set_flags(gb_state, FLAG_N, false);
  set_flags(gb_state, FLAG_H, true);
  set_flags(gb_state, FLAG_Z, ((val >> inst.p1.b3) & 1) == 0);
}

static void ex_scf(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SCF);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  set_flags(gb_state, FLAG_N | FLAG_H, false);
  set_flags(gb_state, FLAG_C, true);
}
static void ex_ccf(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == CCF);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  set_flags(gb_state, FLAG_N | FLAG_H, false);
  set_flags(gb_state, FLAG_C, !(gb_state->regs.f & FLAG_C));
}
static void ex_set(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SET);
  assert(inst.p1.type == B3);
  assert(IS_R8(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  enum r8 reg = inst.p2.r8;
  assert(reg <= R8_A);
  uint8_t bit = inst.p1.b3;
  assert(bit <= 7);
  uint8_t val = get_r8(gb_state, reg);
  val |= (1 << bit);
  set_r8(gb_state, reg, val);
}

static void ex_res(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RES);
  assert(inst.p1.type == B3);
  assert(IS_R8(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  enum r8 reg = inst.p2.r8;
  assert(reg <= R8_A);
  uint8_t bit = inst.p1.b3;
  assert(bit <= 7);
  uint8_t val = get_r8(gb_state, reg);
  val &= ~(1 << bit);
  set_r8(gb_state, reg, val);
}

static void ex_rl(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RL);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t old_carry_flag = (FLAG_C & gb_state->regs.f) >> 4;
  set_flags(gb_state, FLAG_C, (val >> 7) & 1);
  val <<= 1;
  val |= old_carry_flag;
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_H | FLAG_N, false);
  set_flags(gb_state, FLAG_Z, val == 0);
}

static void ex_rr(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RR);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t old_carry_flag = (FLAG_C & gb_state->regs.f) >> 4;
  set_flags(gb_state, FLAG_C, val & 1);
  val >>= 1;
  val |= (old_carry_flag << 7);
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_H | FLAG_N, false);
  set_flags(gb_state, FLAG_Z, val == 0);
}

static void ex_rla(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RLA);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t old_carry_flag = (FLAG_C & gb_state->regs.f) >> 4;
  set_flags(gb_state, FLAG_C, (val >> 7) & 1);
  val <<= 1;
  val |= old_carry_flag;
  set_r8(gb_state, R8_A, val);

  set_flags(gb_state, FLAG_Z | FLAG_H | FLAG_N, false);
}

static void ex_rra(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RRA);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t old_carry_flag = (FLAG_C & gb_state->regs.f) >> 4;
  set_flags(gb_state, FLAG_C, val & 1);
  val >>= 1;
  val |= (old_carry_flag << 7);
  set_r8(gb_state, R8_A, val);

  set_flags(gb_state, FLAG_Z | FLAG_H | FLAG_N, false);
}

static void ex_rlc(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RLC);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t carry = (val >> 7) & 1;
  set_flags(gb_state, FLAG_C, carry);
  val <<= 1;
  val |= carry;
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_Z, val == 0);
  set_flags(gb_state, FLAG_H | FLAG_N, false);
}

static void ex_rrc(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RRC);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t carry = val & 1;
  set_flags(gb_state, FLAG_C, carry);
  val >>= 1;
  val |= (carry << 7);
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_Z, val == 0);
  set_flags(gb_state, FLAG_H | FLAG_N, false);
}

static void ex_rlca(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RLCA);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t carry = (val >> 7) & 1;
  set_flags(gb_state, FLAG_C, carry);
  val <<= 1;
  val |= carry;
  set_r8(gb_state, R8_A, val);

  set_flags(gb_state, FLAG_Z | FLAG_H | FLAG_N, false);
}

static void ex_rrca(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RRCA);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t carry = val & 1;
  set_flags(gb_state, FLAG_C, carry);
  val >>= 1;
  val |= carry << 7;
  set_r8(gb_state, R8_A, val);

  set_flags(gb_state, FLAG_Z | FLAG_H | FLAG_N, false);
}

static void ex_rst(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RST);
  assert(IS_TGT3(inst.p1) && inst.p1.tgt3 < 8);
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(4);
  uint16_t addr = inst.p1.tgt3 * 8;
  push16(gb_state, gb_state->regs.pc);
  gb_state->regs.pc = addr;
}

static void ex_sla(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SLA);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t carry = (val >> 7) & 1;
  set_flags(gb_state, FLAG_C, carry);
  val <<= 1;
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_Z, val == 0);
  set_flags(gb_state, FLAG_H | FLAG_N, false);
}

static void ex_sra(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SRA);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t carry = val & 1;
  set_flags(gb_state, FLAG_C, carry);
  uint8_t b7 = val & (1 << 7);
  val >>= 1;
  val |= b7; // For some reason we leave bit 7 unchanged in sra.
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_Z, val == 0);
  set_flags(gb_state, FLAG_H | FLAG_N, false);
}

static void ex_srl(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SRL);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t carry = val & 1;
  set_flags(gb_state, FLAG_C, carry);
  val >>= 1;
  set_r8(gb_state, inst.p1.r8, val);

  set_flags(gb_state, FLAG_Z, val == 0);
  set_flags(gb_state, FLAG_H | FLAG_N, false);
}

static void ex_swap(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == SWAP);
  assert(IS_R8(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(2);
  if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
  uint8_t val = get_r8(gb_state, inst.p1.r8);
  uint8_t result = ((val & 0x0F) << 4) | ((val & 0xF0) >> 4);
  set_r8(gb_state, inst.p1.r8, result);

  set_flags(gb_state, FLAG_Z, result == 0);
  set_flags(gb_state, FLAG_H | FLAG_N | FLAG_C, false);
}

static void ex_dec(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == DEC);
  assert(IS_R8(inst.p1) || IS_R16(inst.p1));
  assert(IS_VOID(inst.p2));

  if (IS_R8(inst.p1)) {
    SPEND_MCYCLES(1);
    if (inst.p1.r8 == R8_HL_DREF) SPEND_MCYCLES(2);
    uint8_t val;
    val = get_r8(gb_state, inst.p1.r8);
    set_r8(gb_state, inst.p1.r8, val - 1);
    set_flags(gb_state, FLAG_Z, (uint8_t)(val - 1) == 0x00);
    set_flags(gb_state, FLAG_N, 1);
    // if the old val's lower 4 bits are 0 then we know there will be a carry after subtraction
    set_flags(gb_state, FLAG_H, (val & 0x0F) == 0x0);
    return;
  }
  if (IS_R16(inst.p1)) {
    SPEND_MCYCLES(2);
    uint16_t val;
    val = get_r16(gb_state, inst.p1.r16);
    set_r16(gb_state, inst.p1.r16, val - 1);
    // no flags affected for inc r16
    return;
  }
  abort();
}

static void ex_di(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == DI);
  SPEND_MCYCLES(1);
  gb_state->regs.io.ime = false;
}

static void ex_ei(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == EI);
  SPEND_MCYCLES(1);
  gb_state->regs.io.set_ime_after = true;
}

static void ex_daa(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == DAA);
  assert(IS_VOID(inst.p1) && IS_VOID(inst.p2));
  SPEND_MCYCLES(1);
  uint8_t val = get_r8(gb_state, R8_A);
  uint8_t adjust = 0;
  if ((gb_state->regs.f & FLAG_N) != 0) {
    // The last inst was a subtraction.
    if ((gb_state->regs.f & FLAG_H) != 0) adjust += 0x06;
    if ((gb_state->regs.f & FLAG_C) != 0) adjust += 0x60;
    val -= adjust;
  } else {
    // The last inst was an addition.
    if (((gb_state->regs.f & FLAG_H) != 0) || (val & 0x0F) > 0x9) adjust += 0x06;
    if (((gb_state->regs.f & FLAG_C) != 0) || val > 0x99) {
      adjust += 0x60;
      set_flags(gb_state, FLAG_C, true);
    }
    val += adjust;
  }
  set_flags(gb_state, FLAG_H, false);
  set_flags(gb_state, FLAG_Z, val == 0);
  set_r8(gb_state, R8_A, val);
}

static bool eval_condition(struct gb_state *gb_state, const struct inst_param inst_param) {
  assert(inst_param.type == COND);
  switch (inst_param.cond) {
  case COND_NZ: return (gb_state->regs.f & (1 << 7)) == 0;
  case COND_Z: return ((gb_state->regs.f & (1 << 7)) >> 7) == 1;
  case COND_NC: return (gb_state->regs.f & (1 << 4)) == 0;
  case COND_C: return ((gb_state->regs.f & (1 << 4)) >> 4) == 1;
  }
  unreachable();
}

static void ex_ret(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RET);
  assert(IS_VOID(inst.p1) || IS_COND(inst.p1));
  assert(IS_VOID(inst.p2));
  if (inst.p1.type == VOID_PARAM_TYPE) {
    SPEND_MCYCLES(4);
    gb_state->regs.pc = pop16(gb_state);
    return;
  }
  if (inst.p1.type == COND) {
    SPEND_MCYCLES(2);
    if (eval_condition(gb_state, inst.p1)) {
      SPEND_MCYCLES(3);
      gb_state->regs.pc = pop16(gb_state);
    }
    return;
  }
}

static void ex_reti(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == RETI);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(4);
  gb_state->regs.io.ime = true;
  gb_state->regs.pc = pop16(gb_state);
}

void call(struct gb_state *gb_state, uint16_t addr) {
  push16(gb_state, gb_state->regs.pc);
  gb_state->regs.pc = addr;
}

static void ex_call(struct gb_state *gb_state, struct inst inst) {
  if (inst.p1.type == IMM16) {
    SPEND_MCYCLES(6);
    call(gb_state, inst.p1.imm16);
    return;
  } else {
    assert(inst.p1.type == COND);
    assert(inst.p2.type == IMM16);
    SPEND_MCYCLES(3);
    if (eval_condition(gb_state, inst.p1)) {
      SPEND_MCYCLES(3);
      call(gb_state, inst.p2.imm16);
    }
    return;
  }
  unreachable();
}

static void ex_jr(struct gb_state *gb_state, struct inst inst) {
  if (IS_IMM8(inst.p1)) {
    SPEND_MCYCLES(3);
    gb_state->regs.pc += *(int8_t *)&inst.p1.imm8;
    return;
  }
  if (IS_COND(inst.p1)) {
    if (IS_IMM8(inst.p2)) {
      SPEND_MCYCLES(2);
      if (eval_condition(gb_state, inst.p1)) {
        SPEND_MCYCLES(1);

        gb_state->regs.pc += *(int8_t *)&inst.p2.imm8;
      }
      return;
    }
  }
}

static void ex_jp(struct gb_state *gb_state, struct inst inst) {
  switch (inst.p1.type) {
  case IMM16:
    SPEND_MCYCLES(4);
    gb_state->regs.pc = inst.p1.imm16;
    return;
  case COND:
    assert(IS_IMM16(inst.p2));
    SPEND_MCYCLES(3);
    if (eval_condition(gb_state, inst.p1)) {
      SPEND_MCYCLES(1);
      gb_state->regs.pc = inst.p2.imm16;
    }
    return;
  case R16:
    assert(inst.p1.r16 == R16_HL);
    SPEND_MCYCLES(1);
    gb_state->regs.pc = get_r16(gb_state, R16_HL);
    return;
  default: ERR(gb_state, "Unknown JP inst params"); return;
  }
}

static void ex_cp(struct gb_state *gb_state, struct inst inst) {
  // TODO: I'de like for the flags here to be better tested, i'm unsure if I'm
  // doing the carry / half carry flags correctly.
  uint8_t val1;
  uint8_t val2;
  uint16_t res;
  if (inst.p1.type == R8) {
    val1 = get_r8(gb_state, inst.p1.r8);
  } else {
    goto not_implemented;
  }

  if (inst.p2.type == R8) {
    SPEND_MCYCLES(1);
    if (inst.p2.r8 == R8_HL_DREF) SPEND_MCYCLES(1);
    val2 = get_r8(gb_state, inst.p2.r8);
  } else if (inst.p2.type == IMM8) {
    SPEND_MCYCLES(2);
    val2 = inst.p2.imm8;
  } else {
    goto not_implemented;
  }
  res = val1 - val2;

  set_flags(gb_state, FLAG_N, true);
  set_flags(gb_state, FLAG_Z, res == 0);
  set_flags(gb_state, FLAG_H, (val1 & 0x0F) < (val2 & 0x0F));
  set_flags(gb_state, FLAG_C, val1 < val2);
  return;
not_implemented:
  NOT_IMPLEMENTED("Unknown compare instruction");
}
static void ex_cpl(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == CPL);
  assert(IS_VOID(inst.p1));
  assert(IS_VOID(inst.p2));
  SPEND_MCYCLES(1);

  uint8_t val = get_r8(gb_state, R8_A);
  set_r8(gb_state, R8_A, ~val);
  set_flags(gb_state, FLAG_N | FLAG_H, true);
}

void execute(struct gb_state *gb_state, struct inst inst) {
  bool set_ime_after_this_inst = gb_state->regs.io.set_ime_after;
#ifdef PRINT_INST_DURING_EXEC
  print_inst(stdout, inst);
#endif
  switch (inst.type) {
  case ADC: ex_adc(gb_state, inst); break;
  case ADD: ex_add(gb_state, inst); break;
  case AND: ex_and(gb_state, inst); break;
  case BIT: ex_bit(gb_state, inst); break;
  case CALL: ex_call(gb_state, inst); break;
  case CCF: ex_ccf(gb_state, inst); break;
  case CP: ex_cp(gb_state, inst); break;
  case CPL: ex_cpl(gb_state, inst); break;
  case DAA: ex_daa(gb_state, inst); break;
  case DEC: ex_dec(gb_state, inst); break;
  case DI: ex_di(gb_state, inst); break;
  case EI: ex_ei(gb_state, inst); break;
  case INC: ex_inc(gb_state, inst); break;
  case JP: ex_jp(gb_state, inst); break;
  case JR: ex_jr(gb_state, inst); break;
  case LD: ex_ld(gb_state, inst); break;
  case LDH: ex_ldh(gb_state, inst); break;
  case NOP: ex_nop(gb_state, inst); break;
  case OR: ex_or(gb_state, inst); break;
  case POP: ex_pop(gb_state, inst); break;
  case PUSH: ex_push(gb_state, inst); break;
  case RES: ex_res(gb_state, inst); break;
  case RET: ex_ret(gb_state, inst); break;
  case RETI: ex_reti(gb_state, inst); break;
  case RL: ex_rl(gb_state, inst); break;
  case RLA: ex_rla(gb_state, inst); break;
  case RLC: ex_rlc(gb_state, inst); break;
  case RLCA: ex_rlca(gb_state, inst); break;
  case RR: ex_rr(gb_state, inst); break;
  case RRA: ex_rra(gb_state, inst); break;
  case RRC: ex_rrc(gb_state, inst); break;
  case RRCA: ex_rrca(gb_state, inst); break;
  case RST: ex_rst(gb_state, inst); break;
  case SBC: ex_sbc(gb_state, inst); break;
  case SCF: ex_scf(gb_state, inst); break;
  case SET: ex_set(gb_state, inst); break;
  case SLA: ex_sla(gb_state, inst); break;
  case SRA: ex_sra(gb_state, inst); break;
  case SRL: ex_srl(gb_state, inst); break;
  case STOP: ex_stop(gb_state, inst); break;
  case SUB: ex_sub(gb_state, inst); break;
  case SWAP: ex_swap(gb_state, inst); break;
  case XOR: ex_xor(gb_state, inst); break;

  case UNKNOWN_INST:
  default: ERR(gb_state, "`execute()` called with `inst.type` that isn't implemented."); break;
  }
  if (set_ime_after_this_inst) {
    gb_state->regs.io.ime = true;
    gb_state->regs.io.set_ime_after = false;
  }
  if (gb_state->regs.io.ime) {
    // Interupt handlers
    uint8_t to_handle = gb_state->regs.io.ie & gb_state->regs.io.if_;
    if (to_handle & 0b00001) { // vblank handler
      goto interrupt_handled;
    }
    if (to_handle & 0b00010) { // stat handler
      goto interrupt_handled;
    }
    if (to_handle & 0b00100) { // timer handler
      gb_state->regs.io.if_ &= ~0b00100;
      call(gb_state, 0x0050);
      goto interrupt_handled;
    }
    if (to_handle & 0b01000) { // serial handler
      goto interrupt_handled;
    }
    if (to_handle & 0b10000) { // joypad handler
      goto interrupt_handled;
    }
    goto interrupt_handled_end;
  interrupt_handled:
    gb_state->regs.io.ime = false;
  interrupt_handled_end:
  }
}

#ifdef RUN_CPU_TESTS

#include "test_asserts.h"

void test_fetch() {
  struct gb_state gb_state;
  struct inst inst;

  gb_state_init(&gb_state);
  gb_state.regs.pc = 0x0100;

  write_mem8(&gb_state, 0x100, 0b00100001);
  write_mem16(&gb_state, 0x101, 452);
  inst = fetch(&gb_state);
  assert(inst.type == LD);
  assert(inst.p1.type == R16);
  assert(inst.p1.r16 == R16_HL);
  assert(inst.p2.type == IMM16);
  assert(inst.p2.imm16 == 452);

  write_mem8(&gb_state, 0x103, 0b00010010);
  inst = fetch(&gb_state);
  assert(inst.type == LD);
  assert(inst.p1.type == R16_MEM);
  assert(inst.p1.r16 == R16_DE);
  assert(inst.p2.type == R8);
  assert(inst.p2.r8 == R8_A);

  write_mem8(&gb_state, 0x104, 0b00011010);
  inst = fetch(&gb_state);
  assert(inst.type == LD);
  assert(inst.p1.type == R8);
  assert(inst.p1.r8 == R8_A);
  assert(inst.p2.type == R16_MEM);
  assert(inst.p2.r16 == R16_DE);

  write_mem8(&gb_state, 0x105, 0b00001000);
  write_mem16(&gb_state, 0x106, 10403);
  inst = fetch(&gb_state);
  assert(inst.type == LD);
  assert(inst.p1.type == IMM16_MEM);
  assert(inst.p1.imm16 == 10403);
  assert(inst.p2.type == R16);
  assert(inst.p2.r16 == R16_SP);
}

void test_execute_load() {
  struct gb_state gb_state;
  gb_state_init(&gb_state);
  struct inst inst;

  // Load IMM16 into reg BC
  inst = (struct inst){
      .type = LD,
      .p1 = R16_PARAM(R16_BC),
      .p2 = IMM16_PARAM(452),
  };
  execute(&gb_state, inst);
  assert(get_r16(&gb_state, R16_BC) == 452);

  // Load reg A into addr in reg BC
  inst = (struct inst){
      .type = LD,
      .p1 = R16_MEM_PARAM(R16_MEM_BC),
      .p2 = R8_PARAM(R8_A),
  };
  set_r16(&gb_state, R16_BC, 0xC000);
  gb_state.regs.a = 42;
  execute(&gb_state, inst);
  assert(read_mem8(&gb_state, 0xC000) == 42);

  // Load contents of addr in reg BC into reg A
  inst = (struct inst){
      .type = LD,
      .p1 = R8_PARAM(R8_A),
      .p2 = R16_MEM_PARAM(R16_MEM_BC),
  };
  write_mem8(&gb_state, 0xC000, 134);
  execute(&gb_state, inst);
  assert(gb_state.regs.a == 134);

  // Load contents of addr in reg HL into reg A and increment the pointer.
  inst = (struct inst){
      .type = LD,
      .p1 = R8_PARAM(R8_A),
      .p2 = R16_MEM_PARAM(R16_MEM_HLI),
  };
  set_r16(&gb_state, R16_HL, 0xC000);
  write_mem8(&gb_state, 0xC000, 134);
  execute(&gb_state, inst);
  assert(get_r16(&gb_state, R16_HL) == 0xC001);
  assert(gb_state.regs.a == 134);
  // Then load contents of reg A into the addr in reg HL.
  inst = (struct inst){
      .type = LD,
      .p1 = R16_MEM_PARAM(R16_MEM_HLD),
      .p2 = R8_PARAM(R8_A),
  };
  set_r8(&gb_state, R8_A, 21);
  execute(&gb_state, inst);
  assert(read_mem8(&gb_state, 0xC001) == 21);
  assert(get_r16(&gb_state, R16_HL) == 0xC000);

  // Load stack pointer into addr at IMM16
  inst = (struct inst){
      .type = LD,
      .p1 = IMM16_MEM_PARAM(0xC010),
      .p2 = R16_PARAM(R16_SP),
  };
  set_r16(&gb_state, R16_SP, 0xD123);
  execute(&gb_state, inst);
  assert(gb_state.regs.sp == 0xD123);
  assert(read_mem16(&gb_state, 0xC010) == 0xD123);
}

void test_stack_ops() {
  struct gb_state gb_state;
  gb_state_init(&gb_state);

  push16(&gb_state, 0x1234);
  assert_eq(gb_state.regs.sp, 0xDFFE);
  // 16 bit vals on the stack should be little endian so that they can be read
  // like 16 bit values anywhere else in memory.
  assert_eq(read_mem8(&gb_state, 0xDFFF), 0x12);
  assert_eq(read_mem8(&gb_state, 0xDFFE), 0x34);
  assert_eq(read_mem16(&gb_state, 0xDFFE), 0x1234);

  assert_eq(pop16(&gb_state), 0x1234);
}

void test_execute_call_ret() {
  struct gb_state gb_state;
  gb_state_init(&gb_state);
  assert_eq(gb_state.regs.sp, 0xE000);
  gb_state.regs.pc = 0x0190;
  execute(&gb_state, (struct inst){.type = CALL, .p1 = IMM16_PARAM(0x0210), .p2 = VOID_PARAM});
  assert_eq(gb_state.regs.sp, 0xDFFE);
  assert_eq(gb_state.regs.pc, 0x0210);
  assert_eq(read_mem16(&gb_state, 0xDFFE), 0x0190);
  execute(&gb_state, (struct inst){.type = RET, .p1 = VOID_PARAM, .p2 = VOID_PARAM});
  assert_eq(gb_state.regs.sp, 0xE000);
  assert_eq(gb_state.regs.pc, 0x0190);
}

#define TEST_CASE(name)                                                                                                \
  {                                                                                                                    \
    SDL_Log("running `test_%s()`", #name);                                                                             \
    test_##name();                                                                                                     \
  }

int main() {
  SDL_Log("Starting CPU tests.");
  TEST_CASE(fetch);
  TEST_CASE(execute_load);
  TEST_CASE(execute_call_ret);
  TEST_CASE(stack_ops);
  SDL_Log("CPU tests succeeded.");
  SDL_Quit();
}

#endif
