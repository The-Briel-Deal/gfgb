#include "cpu.h"
#include "common.h"

#include <assert.h>
#include <stdint.h>

#define R16_PARAM(r)                                                           \
  (struct inst_param) { .type = R16, .r16 = r }
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

static inline uint16_t get_r16(struct gb_state *gb_state, uint8_t r16) {
  switch (r16) {
  case 0: // bc
    return COMBINED_REG(gb_state->regs, b, c);
  case 1: // de
    return COMBINED_REG(gb_state->regs, d, e);
  case 2: // hl
    return COMBINED_REG(gb_state->regs, h, l);
  case 3: // sp
    return gb_state->regs.sp;
  default:
    // bc, de, hl, and sp are the only valid r16 registers.
    exit(1);
  }
}

static inline void set_r16(struct gb_state *gb_state, uint8_t r16,
                           uint16_t val) {
  switch (r16) {
  case 0: // bc
    gb_state->regs.b = (0x00FF & val) >> 0;
    gb_state->regs.c = (0xFF00 & val) >> 8;
    return;
  case 1: // de
    gb_state->regs.d = (0x00FF & val) >> 0;
    gb_state->regs.e = (0xFF00 & val) >> 8;
    return;
  case 2: // hl
    gb_state->regs.h = (0x00FF & val) >> 0;
    gb_state->regs.l = (0xFF00 & val) >> 8;
    return;
  case 3: // sp
    gb_state->regs.sp = val;
    return;
  default:
    // bc, de, hl, and sp are the only valid r16 registers.
    exit(1);
  }
}

struct inst fetch(struct gb_state *gb_state) {
  uint8_t curr_byte = next8(gb_state);
  uint8_t block = CRUMB0(curr_byte);
  switch (block) {
  case 0:
    if (curr_byte == 0b00000000) return (struct inst){.type = NOP};
    switch (NIBBLE1(curr_byte)) {
    case 0b0001: {
      return (struct inst){.type = LD,
                           .p1 = R16_PARAM(CRUMB1(curr_byte)),
                           .p2 = IMM16_PARAM(next16(gb_state))};
    }
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
void execute(struct gb_state *gb_state, struct inst inst) {
  switch (inst.type) {
  case NOP: return;
  case LD:
    switch (inst.p1.type) {
    case R16: {
      uint8_t reg_dest = inst.p1.r16;
      switch (inst.p2.type) {
      case IMM16: {
        uint16_t src_val = inst.p2.imm16;
        set_r16(gb_state, reg_dest, src_val);
        return;
      }
      default: break;
      }
    }
    default: break;
    }
    break;
  default: break;
  }
  NOT_IMPLEMENTED(
      "`execute()` called with `inst.type` that isn't implemented.");
}

#ifdef RUN_TESTS

void test_fetch() {
  struct gb_state gb_state;
  gb_state_init(&gb_state);

  write_mem8(&gb_state, 0x100, 0b00100001);
  write_mem16(&gb_state, 0x101, 452);

  struct inst inst = fetch(&gb_state);
  assert(inst.type == LD);
  assert(inst.p1.type == R16);
  assert(inst.p1.r16 == 0b10);
  assert(inst.p2.type == IMM16);
  assert(inst.p2.imm16 == 452);
}

void test_execute() {
  struct gb_state gb_state;
  gb_state_init(&gb_state);

  // LD r16=BC imm16=452
  write_mem8(&gb_state, 0x100, 0b00000001);
  write_mem16(&gb_state, 0x101, 452);

  struct inst inst = fetch(&gb_state);
  assert(COMBINED_REG(gb_state.regs, b, c) == 0);
  execute(&gb_state, inst);
  assert(COMBINED_REG(gb_state.regs, b, c) == 452);
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
