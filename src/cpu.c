#include "cpu.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define R8_PARAM(r)                                                            \
  (struct inst_param) { .type = R8, .r8 = r }
#define R16_PARAM(r)                                                           \
  (struct inst_param) { .type = R16, .r16 = r }
#define R16_MEM_PARAM(r)                                                       \
  (struct inst_param) { .type = R16_MEM, .r16_mem = r }
#define IMM16_PARAM(imm)                                                       \
  (struct inst_param) { .type = IMM16, .imm16 = imm }
#define IMM16_MEM_PARAM(imm)                                                   \
  (struct inst_param) { .type = IMM16_MEM, .imm16 = imm }

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
  default: abort();
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
  default: abort();
  }
}

static inline uint16_t get_r16(struct gb_state *gb_state, enum r16 r16) {
  switch (r16) {
  case R16_BC: return COMBINED_REG(gb_state->regs, b, c);
  case R16_DE: return COMBINED_REG(gb_state->regs, d, e);
  case R16_HL: return COMBINED_REG(gb_state->regs, h, l);
  case R16_SP: return gb_state->regs.sp;
  default: abort(); // bc, de, hl, and sp are the only valid r16 registers.
  }
}

static inline void set_r16(struct gb_state *gb_state, enum r16 r16,
                           uint16_t val) {
  switch (r16) {
  case R16_BC: SET_COMBINED_REG(gb_state->regs, b, c, val); return;
  case R16_DE: SET_COMBINED_REG(gb_state->regs, d, e, val); return;
  case R16_HL: SET_COMBINED_REG(gb_state->regs, h, l, val); return;
  case R16_SP: gb_state->regs.sp = val; return;
  default: abort(); // bc, de, hl, and sp are the only valid r16 registers.
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

static inline uint8_t *get_r16_mem_addr(struct gb_state *gb_state,
                                        enum r16_mem r16_mem) {
  assert(r16_mem <= R16_MEM_HLD);
  uint16_t addr;
  switch (r16_mem) {
  case R16_MEM_BC: return unmap_address(gb_state, get_r16(gb_state, R16_BC));
  case R16_MEM_DE: return unmap_address(gb_state, get_r16(gb_state, R16_DE));
  case R16_MEM_HLI: // Increment HL after deref
    addr = get_r16(gb_state, R16_HL);
    set_r16(gb_state, R16_HL, addr + 1);
    return unmap_address(gb_state, addr);
  case R16_MEM_HLD: // Decrement HL after deref
    addr = get_r16(gb_state, R16_HL);
    set_r16(gb_state, R16_HL, addr - 1);
    return unmap_address(gb_state, addr);
  }
  abort(); // This should never happen unless something is very wrong.
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
    case 0b1000:
      if (CRUMB1(curr_byte) == 0b00)
        return (struct inst){
            .type = LD,
            .p1 = IMM16_MEM_PARAM(next16(gb_state)),
            .p2 = R16_PARAM(R16_SP),
        };
      break;
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

#define IS_R16(param)       (param.type == R16)
#define IS_R16_MEM(param)   (param.type == R16_MEM)
#define IS_R8(param)        (param.type == R8)
#define IS_IMM16(param)     (param.type == IMM16)
#define IS_IMM16_MEM(param) (param.type == IMM16_MEM)

static void print_inst(const struct inst inst) {
  switch (inst.type) {
  case NOP: printf("NOP"); break;
  case LD: printf("LD"); break;
  case UNKNOWN_INST: printf("UNKNOWN"); break;
  }

  printf("\n");
}

static void disassemble_rom(FILE *rom) {
  struct gb_state gb_state;
  gb_state_init(&gb_state);

  // 16KB is the size of ROM bank 0 without any banks mapped via the mapper.
  // TODO: Make this work for mapped banks once they are implemented.
  uint16_t max_rom_size = KB(16);
  size_t size_read =
      fread(gb_state.rom0, sizeof(*gb_state.rom0), max_rom_size, rom);
  if (ferror(rom) != 0) abort();
  while (gb_state.regs.pc < size_read) {
    struct inst inst = fetch(&gb_state);
    print_inst(inst);
  }
}

void ex_ld(struct gb_state *gb_state, struct inst inst) {
  struct inst_param dest = inst.p1;
  struct inst_param src = inst.p2;
  if (IS_R16(dest) && IS_IMM16(src)) {
    set_r16(gb_state, dest.r16, src.imm16);
    return;
  }
  if (IS_R16_MEM(dest) && IS_R8(src)) {
    *get_r16_mem_addr(gb_state, dest.r16_mem) = get_r8(gb_state, src.r8);
    return;
  }
  if (IS_R8(dest) && IS_R16_MEM(src)) {
    set_r8(gb_state, dest.r8, *get_r16_mem_addr(gb_state, src.r16_mem));
    return;
  }
  if (IS_IMM16_MEM(dest) && IS_R16(src)) {
    write_mem16(gb_state, dest.imm16, get_r16(gb_state, src.r16));
    return;
  }
}

#undef IS_R16
#undef IS_R16_MEM
#undef IS_R8
#undef IS_IMM16
#undef IS_IMM16_MEM

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

void test_execute() { test_execute_load(); }

int main() {
  SDL_Log("Starting CPU tests.");
  SDL_Log("running `test_fetch()`");
  test_fetch();
  SDL_Log("running `test_execute()`");
  test_execute();
  SDL_Log("CPU tests succeeded.");
  SDL_Quit();
}

#endif
