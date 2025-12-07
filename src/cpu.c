#include "cpu.h"
#include "common.h"

#include <assert.h>
#include <stdint.h>

#define R8_PARAM(r)                                                            \
  (struct inst_param) { .type = R8, .r8 = r }
#define R16_PARAM(r)                                                           \
  (struct inst_param) { .type = R16, .r16 = r }
#define R16_MEM_PARAM(r)                                                       \
  (struct inst_param) { .type = R16_MEM, .r16 = r }
#define IMM16_PARAM(imm)                                                       \
  (struct inst_param) { .type = IMM16, .imm16 = imm }

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
  switch (r8) {
  case R8_B: return gb_state->regs.b;
  case R8_C: return gb_state->regs.c;
  case R8_D: return gb_state->regs.d;
  case R8_E: return gb_state->regs.e;
  case R8_H: return gb_state->regs.h;
  case R8_L: return gb_state->regs.l;
  case R8_HL_DREF: NOT_IMPLEMENTED("R8_HL_DREF not yet implemented.");
  case R8_A: return gb_state->regs.a;
  default: exit(1);
  }
}

static inline void set_r8(struct gb_state *gb_state, enum r8 r8, uint8_t val) {
  switch (r8) {
  case R8_B: gb_state->regs.b = val; return;
  case R8_C: gb_state->regs.c = val; return;
  case R8_D: gb_state->regs.d = val; return;
  case R8_E: gb_state->regs.e = val; return;
  case R8_H: gb_state->regs.h = val; return;
  case R8_L: gb_state->regs.l = val; return;
  case R8_HL_DREF: NOT_IMPLEMENTED("R8_HL_DREF not yet implemented.");
  case R8_A: gb_state->regs.a = val; return;
  default: exit(1);
  }
}

static inline uint16_t get_r16(struct gb_state *gb_state, enum r16 r16) {
  switch (r16) {
  case R16_BC: return COMBINED_REG(gb_state->regs, b, c);
  case R16_DE: return COMBINED_REG(gb_state->regs, d, e);
  case R16_HL: return COMBINED_REG(gb_state->regs, h, l);
  case R16_SP: return gb_state->regs.sp;
  default: exit(1); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

static inline void set_r16(struct gb_state *gb_state, enum r16 r16,
                           uint16_t val) {
  switch (r16) {
  case R16_BC: SET_COMBINED_REG(gb_state->regs, b, c, val); return;
  case R16_DE: SET_COMBINED_REG(gb_state->regs, d, e, val); return;
  case R16_HL: SET_COMBINED_REG(gb_state->regs, h, l, val); return;
  case R16_SP: gb_state->regs.sp = val; return;
  default: exit(1); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

static inline void set_r16_mem(struct gb_state *gb_state, enum r16 r16,
                               uint8_t val) {
  uint16_t mem_offset;
  switch (r16) {
  case R16_BC: mem_offset = COMBINED_REG(gb_state->regs, b, c); break;
  case R16_DE: mem_offset = COMBINED_REG(gb_state->regs, d, e); break;
  case R16_HL: mem_offset = COMBINED_REG(gb_state->regs, h, l); break;
  case R16_SP: mem_offset = gb_state->regs.sp; break;
  default: exit(1); // bc, de, hl, and sp are the only valid r16 registers.
  }
  write_mem8(gb_state, mem_offset, val);
}

struct inst fetch(struct gb_state *gb_state) {
  uint8_t curr_byte = next8(gb_state);
  uint8_t block = CRUMB0(curr_byte);
  switch (block) {
  case 0:
    if (curr_byte == 0b00000000) return (struct inst){.type = NOP};
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
    case 0b1010:
      return (struct inst){
          .type = LD,
          .p1 = R8_PARAM(R8_A),
          .p2 = R16_MEM_PARAM(CRUMB1(curr_byte)),
      };
    }
    break;
  case 1: break;
  case 2: break;
  case 3: break;
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown instruction 0x%.4x.",
               curr_byte);
  NOT_IMPLEMENTED("Instruction not implemented.");
}

#define IS_R16(param)     (param.type == R16)
#define IS_R16_MEM(param) (param.type == R16_MEM)
#define IS_R8(param)      (param.type == R8)
#define IS_IMM16(param)   (param.type == IMM16)

void ex_ld(struct gb_state *gb_state, struct inst inst) {
  struct inst_param dest = inst.p1;
  struct inst_param src = inst.p2;
  if (IS_R16(dest) && IS_IMM16(src)) {
    set_r16(gb_state, dest.r16, src.imm16);
    return;
  }
  if (IS_R16_MEM(dest) && IS_R8(src)) {
    set_r16_mem(gb_state, dest.r16, get_r8(gb_state, src.r8));
    return;
  }
  if (IS_R8(dest) && IS_R16_MEM(src)) {
    set_r8(gb_state, dest.r8, read_mem8(gb_state, get_r16(gb_state, src.r16)));
    return;
  }
}

#undef IS_R16
#undef IS_R16_MEM
#undef IS_R8
#undef IS_IMM16

void execute(struct gb_state *gb_state, struct inst inst) {
  switch (inst.type) {
  case NOP: return;
  case LD: ex_ld(gb_state, inst); return;
  default: break;
  }
  NOT_IMPLEMENTED(
      "`execute()` called with `inst.type` that isn't implemented.");
}

#ifdef RUN_TESTS

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
}

void test_execute() {
  struct gb_state gb_state;
  gb_state_init(&gb_state);

  // LD r16=BC imm16=452
  write_mem8(&gb_state, 0x100, 0b00000001);
  write_mem16(&gb_state, 0x101, 452);
  struct inst inst;
  inst = fetch(&gb_state);
  assert(get_r16(&gb_state, R16_BC) == 0);
  execute(&gb_state, inst);
  assert(get_r16(&gb_state, R16_BC) == 452);

  // LD r16_mem=*BC r8=a
  set_r16(&gb_state, R16_BC, 0xC000);
  gb_state.regs.a = 42;
  write_mem8(&gb_state, 0x103, 0b00000010);
  inst = fetch(&gb_state);
  assert(read_mem8(&gb_state, 0xC000) == 0);
  execute(&gb_state, inst);
  assert(read_mem8(&gb_state, 0xC000) == 42);
}

int main() {
  SDL_Log("Starting CPU tests.");
  SDL_Log("running `test_fetch()`");
  test_fetch();
  SDL_Log("running `test_execute()`");
  test_execute();
  SDL_Log("CPU tests succeeded.");
}

#endif
