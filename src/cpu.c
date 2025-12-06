#include "cpu.h"
#include "common.h"

struct inst fetch(struct gb_state *gb_state) {
  uint8_t curr_byte = read_mem8(gb_state, gb_state->regs.pc);
  gb_state->regs.pc += 1;
  uint8_t block = CRUMB0(curr_byte);
  switch (block) {
  case 0:
    if (curr_byte == 0b00000000) return (struct inst){.inst_type = NOP};
    switch (NIBBLE1(curr_byte)) {
    case 0b0001: {
      struct inst inst = {.inst_type = LD,
                          .p1.type = R16,
                          .p1.r16 = CRUMB1(curr_byte),
                          .p2.type = IMM16,
                          .p2.imm16 = read_mem16(gb_state, gb_state->regs.pc)};
      gb_state->regs.pc += 2;
      return inst;
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
void execute(struct gb_state *gb_state, struct inst inst) {}
