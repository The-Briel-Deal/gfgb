#include "cpu.h"
#include "common.h"

#include <assert.h>
#include <stdio.h>

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
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown instruction 0x%0.4x.",
               curr_byte);
  NOT_IMPLEMENTED("Instruction not implemented.");
}
void execute(struct gb_state *gb_state, struct inst inst) {

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

int main() { 
  printf("Starting CPU tests.\n");
  test_fetch();
  printf("CPU tests succeeded\n");
}

#endif
