#include "cpu.h"
#include "common.h"
#include "disassemble.h"
#include "test_asserts.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum flags : uint8_t {
  FLAG_Z = (1 << 7),
  FLAG_N = (1 << 6),
  FLAG_H = (1 << 5),
  FLAG_C = (1 << 4),
};

#define R8_PARAM(r)                                                            \
  (struct inst_param) { .type = R8, .r8 = r }
#define R16_PARAM(r)                                                           \
  (struct inst_param) { .type = R16, .r16 = r }
#define R16_MEM_PARAM(r)                                                       \
  (struct inst_param) { .type = R16_MEM, .r16_mem = r }
#define R16_STK_PARAM(r)                                                       \
  (struct inst_param) { .type = R16_STK, .r16_stk = r }
#define IMM16_PARAM(imm)                                                       \
  (struct inst_param) { .type = IMM16, .imm16 = imm }
#define SP_IMM8_PARAM(imm)                                                     \
  (struct inst_param) { .type = SP_IMM8, .imm8 = imm }
#define IMM8_PARAM(imm)                                                        \
  (struct inst_param) { .type = IMM8, .imm8 = imm }
#define IMM16_MEM_PARAM(imm)                                                   \
  (struct inst_param) { .type = IMM16_MEM, .imm16 = imm }
#define COND_PARAM(cond)                                                       \
  (struct inst_param) { .type = COND, .r8 = cond }
#define UNKNOWN_INST_BYTE_PARAM(b)                                             \
  (struct inst_param) { .type = UNKNOWN_INST_BYTE, .unknown_inst_byte = b }
#define VOID_PARAM                                                             \
  (struct inst_param) { .type = VOID_PARAM_TYPE }

static inline uint8_t next8(struct gb_state *gb_state) {
  assert(gb_state->regs.pc < sizeof(gb_state->rom0));
  uint8_t val = read_mem8(gb_state, gb_state->regs.pc);
  gb_state->regs.pc += 1;
  return val;
}

static inline uint16_t next16(struct gb_state *gb_state) {
  assert(gb_state->regs.pc < sizeof(gb_state->rom0));
  uint16_t val = read_mem16(gb_state, gb_state->regs.pc);
  gb_state->regs.pc += 2;
  return val;
}

static inline uint8_t get_r8(struct gb_state *gb_state, enum r8 r8) {
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

static inline void set_r8(struct gb_state *gb_state, enum r8 r8, uint8_t val) {
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

static inline uint16_t get_r16(struct gb_state *gb_state, enum r16 r16) {
  struct regs *r = &gb_state->regs;
  switch (r16) {
  case R16_BC: return COMBINED_REG((*r), b, c);
  case R16_DE: return COMBINED_REG((*r), d, e);
  case R16_HL: return COMBINED_REG((*r), h, l);
  case R16_SP: return r->sp;
  default: abort(); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

static inline void set_r16(struct gb_state *gb_state, enum r16 r16,
                           uint16_t val) {
  struct regs *r = &gb_state->regs;
  switch (r16) {
  case R16_BC: SET_COMBINED_REG((*r), b, c, val); return;
  case R16_DE: SET_COMBINED_REG((*r), d, e, val); return;
  case R16_HL: SET_COMBINED_REG((*r), h, l, val); return;
  case R16_SP: r->sp = val; return;
  default: abort(); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

static inline void set_r16_mem(struct gb_state *gb_state, enum r16 r16,
                               uint8_t val) {
  struct regs *r = &gb_state->regs;
  uint16_t mem_offset;
  switch (r16) {
  case R16_BC: mem_offset = COMBINED_REG((*r), b, c); break;
  case R16_DE: mem_offset = COMBINED_REG((*r), d, e); break;
  case R16_HL: mem_offset = COMBINED_REG((*r), h, l); break;
  case R16_SP: mem_offset = r->sp; break;
  default: exit(1); // bc, de, hl, and sp are the only valid r16 registers.
  }
  write_mem8(gb_state, mem_offset, val);
}

static inline uint16_t get_r16_mem(struct gb_state *gb_state,
                                   enum r16_mem r16_mem) {
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
static inline uint16_t get_r16_stk(struct gb_state *gb_state,
                                   enum r16_stk r16_stk) {
  assert(r16_stk <= R16_STK_AF);
  switch (r16_stk) {
  case R16_STK_BC: return get_r16(gb_state, R16_BC);
  case R16_STK_DE: return get_r16(gb_state, R16_DE);
  case R16_STK_HL: return get_r16(gb_state, R16_HL);
  case R16_STK_AF: return COMBINED_REG(gb_state->regs, a, f);
  }
  abort(); // This should never happen unless something is very wrong.
}
static inline void set_r16_stk(struct gb_state *gb_state, enum r16_stk r16_stk,
                               uint16_t val) {
  struct regs *r = &gb_state->regs;
  switch (r16_stk) {
  case R16_STK_BC: SET_COMBINED_REG((*r), b, c, val); return;
  case R16_STK_DE: SET_COMBINED_REG((*r), d, e, val); return;
  case R16_STK_HL: SET_COMBINED_REG((*r), h, l, val); return;
  case R16_STK_AF: SET_COMBINED_REG((*r), a, f, val); return;
  default: abort(); // bc, de, hl, and af are the only valid r16_stk registers.
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
    if (curr_byte == 0b00000000)
      return (struct inst){.type = NOP, .p1 = VOID_PARAM, .p2 = VOID_PARAM};
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
      return (struct inst){.type = LD,
                           .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3),
                           .p2 = IMM8_PARAM(next8(gb_state))};
    // inc r8
    if ((curr_byte & 0b00000111) == 0b00000100)
      return (struct inst){.type = INC,
                           .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3),
                           .p2 = VOID_PARAM};
    // dec r8
    if ((curr_byte & 0b00000111) == 0b00000101)
      return (struct inst){.type = DEC,
                           .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3),
                           .p2 = VOID_PARAM};

    // jr imm8
    if (curr_byte == 0b00011000)
      return (struct inst){
          .type = JR, .p1 = IMM8_PARAM(next8(gb_state)), .p2 = VOID_PARAM};
    // jr cond, imm8
    if ((curr_byte & ~CONDITION_CODE_MASK) == 0b00100000)
      return (struct inst){
          .type = JR,
          .p1 = COND_PARAM((curr_byte & CONDITION_CODE_MASK) >> 3),
          .p2 = IMM8_PARAM(next8(gb_state))};
    break;
  case /* block */ 1:
    return (struct inst){.type = LD,
                         .p1 = R8_PARAM((curr_byte & 0b00111000) >> 3),
                         .p2 = R8_PARAM((curr_byte & 0b00000111) >> 0)};

  case /* block */ 2: {
    struct inst inst = {.type = UNKNOWN_INST,
                        .p1 = R8_PARAM(R8_A),
                        .p2 = R8_PARAM((curr_byte & ARITHMETIC_R8_MASK) >> 0)};
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
    if (NIBBLE1(curr_byte) == 0b0001) // Pop r16stk
      return (struct inst){.type = POP,
                           .p1 = R16_STK_PARAM(CRUMB1(curr_byte)),
                           .p2 = VOID_PARAM};
    if (NIBBLE1(curr_byte) == 0b0101) // Push r16stk
      return (struct inst){.type = PUSH,
                           .p1 = R16_STK_PARAM(CRUMB1(curr_byte)),
                           .p2 = VOID_PARAM};
    if (curr_byte == 0b11000011) // Unconditional jump
      return (struct inst){
          .type = JP, .p1 = IMM16_PARAM(next16(gb_state)), .p2 = VOID_PARAM};

    if (curr_byte == 0b11001001) // RET
      return (struct inst){.type = RET, .p1 = VOID_PARAM, .p2 = VOID_PARAM};

    if (curr_byte == 0b11001101) // Unconditional call
      return (struct inst){
          .type = CALL, .p1 = IMM16_PARAM(next16(gb_state)), .p2 = VOID_PARAM};

    if (curr_byte == 0b11101010) // LD [IMM16], A
      return (struct inst){.type = LD,
                           .p1 = IMM16_MEM_PARAM(next16(gb_state)),
                           .p2 = R8_PARAM(R8_A)};

    if (curr_byte == 0b11111000) // LD HL, SP+IMM8
      return (struct inst){.type = LD,
                           .p1 = R16_PARAM(R16_HL),
                           .p2 = SP_IMM8_PARAM(next8(gb_state))};

    if (curr_byte == 0b11111010) // LD A, [IMM16]
      return (struct inst){.type = LD,
                           .p1 = R8_PARAM(R8_A),
                           .p2 = IMM16_MEM_PARAM(next16(gb_state))};

    if (curr_byte == 0b11111110) // CP A, IMM8
      return (struct inst){
          .type = CP, .p1 = R8_PARAM(R8_A), .p2 = IMM8_PARAM(next8(gb_state))};

    if ((curr_byte & ~CONDITION_CODE_MASK) == 0b11000010) // JP COND, IMM16
      return (struct inst){
          .type = JP,
          .p1 = COND_PARAM((curr_byte & CONDITION_CODE_MASK) >> 3),
          .p2 = IMM16_PARAM(next16(gb_state))};

    break;
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown instruction 0x%.4X.",
               curr_byte);
  return (struct inst){.type = UNKNOWN_INST,
                       .p1 = UNKNOWN_INST_BYTE_PARAM(curr_byte),
                       .p2 = VOID_PARAM};
}

#define IS_R16(param)       (param.type == R16)
#define IS_R16_MEM(param)   (param.type == R16_MEM)
#define IS_R8(param)        (param.type == R8)
#define IS_IMM16(param)     (param.type == IMM16)
#define IS_IMM16_MEM(param) (param.type == IMM16_MEM)
#define IS_IMM8(param)      (param.type == IMM8)
#define IS_SP_IMM8(param)   (param.type == SP_IMM8)
#define IS_COND(param)      (param.type == COND)
#define IS_VOID(param)      (param.type == VOID_PARAM_TYPE)

static void ex_ld(struct gb_state *gb_state, struct inst inst) {
  struct inst_param dest = inst.p1;
  struct inst_param src = inst.p2;
  if (IS_R16(dest) && IS_IMM16(src)) {
    set_r16(gb_state, dest.r16, src.imm16);
    return;
  }
  if (IS_R16_MEM(dest) && IS_R8(src)) {
    write_mem8(gb_state, get_r16_mem(gb_state, dest.r16_mem),
               get_r8(gb_state, src.r8));
    return;
  }
  if (IS_R8(dest)) {
    uint8_t src_val;
    if (IS_R16_MEM(src))
      src_val = read_mem8(gb_state, get_r16_mem(gb_state, src.r16_mem));
    else if (IS_IMM16_MEM(src))
      src_val = read_mem8(gb_state, src.imm16);
    else if (IS_IMM8(src))
      src_val = src.imm8;
    else if (IS_R8(src))
      src_val = get_r8(gb_state, src.r8);
    else
      goto not_implemented;
    set_r8(gb_state, dest.r8, src_val);
    return;
  }
  if (IS_IMM16_MEM(dest) && IS_R16(src)) {
    write_mem16(gb_state, dest.imm16, get_r16(gb_state, src.r16));
    return;
  }
  if (IS_IMM16_MEM(dest) && IS_R8(src)) {
    write_mem8(gb_state, dest.imm16, get_r8(gb_state, src.r8));
    return;
  }
  if (IS_R16(dest) && IS_SP_IMM8(src)) {
    uint16_t src_val = get_r16(gb_state, R16_SP) + src.imm8;
    assert(dest.r16 == R16_HL); // this inst should always be setting HL
    set_r16(gb_state, dest.r16, src_val);
    return;
  }
not_implemented:
  NOT_IMPLEMENTED("Unknown load instruction");
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
  set_r16_stk(gb_state, inst.p1.r16_stk, pop16(gb_state));
}

static void set_flags(struct gb_state *gb_state, enum flags flags, bool on) {
  if (on) {
    gb_state->regs.f |= flags;
  } else {
    gb_state->regs.f &= ~flags;
  }
}

static void ex_inc(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == INC);
  assert(IS_R8(inst.p1) || IS_R16(inst.p1));
  assert(IS_VOID(inst.p2));

  if (IS_R8(inst.p1)) {
    uint8_t val;
    val = get_r8(gb_state, inst.p1.r8);
    set_r8(gb_state, inst.p1.r8, val + 1);
    set_flags(gb_state, FLAG_Z, val + 1 == 0x00);
    set_flags(gb_state, FLAG_N, 0);
    set_flags(gb_state, FLAG_H, ((val & 0x0F) + 1) == 0x10);
    return;
  }
  if (IS_R16(inst.p1)) {
    uint16_t val;
    val = get_r16(gb_state, inst.p1.r16);
    set_r16(gb_state, inst.p1.r16, val + 1);
    // no flags affected for inc r16
    return;
  }
  abort();
}

static void ex_or(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == OR);
  assert(IS_R8(inst.p1) && inst.p1.r8 == R8_A);
  assert(IS_R8(inst.p2) || IS_IMM8(inst.p2));
  struct regs *r = &gb_state->regs;
  uint8_t p2_val;
  if (IS_R8(inst.p2)) {
    p2_val = get_r8(gb_state, inst.p2.r8);
  } else if (IS_IMM8(inst.p2)) {
    p2_val = inst.p2.imm8;
  } else {
    unreachable();
  }
  r->a |= p2_val;
  set_flags(gb_state, FLAG_H | FLAG_N | FLAG_C, false);
  set_flags(gb_state, FLAG_Z, r->a == 0);
}

static void ex_dec(struct gb_state *gb_state, struct inst inst) {
  assert(inst.type == DEC);
  assert(IS_R8(inst.p1) || IS_R16(inst.p1));
  assert(IS_VOID(inst.p2));

  if (IS_R8(inst.p1)) {
    uint8_t val;
    val = get_r8(gb_state, inst.p1.r8);
    set_r8(gb_state, inst.p1.r8, val - 1);
    set_flags(gb_state, FLAG_Z, val - 1 == 0x00);
    set_flags(gb_state, FLAG_N, 1);
    set_flags(gb_state, FLAG_H, ((val & 0xF0) - 1) == 0x0F);
    return;
  }
  if (IS_R16(inst.p1)) {
    uint16_t val;
    val = get_r16(gb_state, inst.p1.r16);
    set_r16(gb_state, inst.p1.r16, val - 1);
    // no flags affected for inc r16
    return;
  }
  abort();
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
    val2 = get_r8(gb_state, inst.p2.r8);
  } else if (inst.p2.type == IMM8) {
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

static bool eval_condition(struct gb_state *gb_state,
                           const struct inst_param inst_param) {
  assert(inst_param.type == COND);
  switch (inst_param.cond) {
  case COND_NZ: return (gb_state->regs.f & (1 << 7)) == 0;
  case COND_Z: return ((gb_state->regs.f & (1 << 7)) >> 7) == 1;
  case COND_NC: return (gb_state->regs.f & (1 << 4)) == 0;
  case COND_C: return ((gb_state->regs.f & (1 << 4)) >> 4) == 1;
  }
  // Something is very wrong if the above switch statement doesn't catch.
  abort();
}

void execute(struct gb_state *gb_state, struct inst inst) {
  switch (inst.type) {
  case NOP: return;
  case LD: ex_ld(gb_state, inst); return;
  case JP: {
    if (IS_IMM16(inst.p1)) {
      gb_state->regs.pc = inst.p1.imm16;
      return;
    }
    if (IS_COND(inst.p1)) {
      if (IS_IMM16(inst.p2)) {
        if (eval_condition(gb_state, inst.p1)) {
          gb_state->regs.pc = inst.p2.imm16;
        }
        return;
      }
    }
    break;
  }
  case JR: {
    if (IS_IMM8(inst.p1)) {
      gb_state->regs.pc += *(int8_t *)&inst.p1.imm8;
      return;
    }
    if (IS_COND(inst.p1)) {
      if (IS_IMM8(inst.p2)) {
        if (eval_condition(gb_state, inst.p1)) {
          gb_state->regs.pc += *(int8_t *)&inst.p2.imm8;
        }
        return;
      }
    }
    break;
  }
  case CALL: {
    if (inst.p1.type == IMM16) {
      push16(gb_state, gb_state->regs.pc);
      gb_state->regs.pc = inst.p1.imm16;
      return;
    }
    break;
  }
  case RET: {
    if (inst.p1.type == VOID_PARAM_TYPE) {
      gb_state->regs.pc = pop16(gb_state);
      return;
    }
    break;
  }
  case CP: ex_cp(gb_state, inst); return;
  case PUSH: ex_push(gb_state, inst); return;
  case POP: ex_pop(gb_state, inst); return;
  case INC: ex_inc(gb_state, inst); return;
  case DEC: ex_dec(gb_state, inst); return;
  case OR: ex_or(gb_state, inst); return;
  default: break;
  }
  NOT_IMPLEMENTED(
      "`execute()` called with `inst.type` that isn't implemented.");
}

#ifdef RUN_CPU_TESTS

void test_fetch() {
  struct gb_state gb_state;
  struct inst inst;

  gb_state_init(&gb_state);

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
  execute(
      &gb_state,
      (struct inst){.type = CALL, .p1 = IMM16_PARAM(0x0210), .p2 = VOID_PARAM});
  assert_eq(gb_state.regs.sp, 0xDFFE);
  assert_eq(gb_state.regs.pc, 0x0210);
  assert_eq(read_mem16(&gb_state, 0xDFFE), 0x0190);
  execute(&gb_state,
          (struct inst){.type = RET, .p1 = VOID_PARAM, .p2 = VOID_PARAM});
  assert_eq(gb_state.regs.sp, 0xE000);
  assert_eq(gb_state.regs.pc, 0x0190);
}

#define TEST_CASE(name)                                                        \
  {                                                                            \
    SDL_Log("running `test_%s()`", #name);                                     \
    test_##name();                                                             \
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
