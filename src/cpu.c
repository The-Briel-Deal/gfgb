#include "cpu.h"
#include "common.h"

struct inst fetch(struct gb_state *gb_state) {
  uint8_t curr_byte = read_mem8(gb_state, gb_state->regs.pc);
  uint8_t block = CRUMB0(curr_byte);
  switch (block) {
  case 0: break;
  case 1: break;
  case 2: break;
  case 3: break;
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown instruction 0x%0.4x.",
               curr_byte);
  NOT_IMPLEMENTED("Instruction not implemented.");
}
void execute(struct gb_state *gb_state, struct inst inst) {}
