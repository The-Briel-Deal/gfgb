#include "cpu.h"
#include "common.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  case 3:
    if (curr_byte == 0b11000011)
      return (struct inst){
          .type = JP, .p1 = IMM16_PARAM(next16(gb_state)), .p2 = VOID_PARAM};
    break;
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unknown instruction 0x%.4x.",
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

#define PRINT_ENUM_CASE(enum_case)                                             \
  case enum_case: fprintf(stream, " " #enum_case); break;

static void print_inst_param(FILE *stream, const struct inst_param inst_param) {
  switch (inst_param.type) {
  case R8:
    switch (inst_param.r8) {
      PRINT_ENUM_CASE(R8_B)
      PRINT_ENUM_CASE(R8_C)
      PRINT_ENUM_CASE(R8_D)
      PRINT_ENUM_CASE(R8_E)
      PRINT_ENUM_CASE(R8_H)
      PRINT_ENUM_CASE(R8_L)
      PRINT_ENUM_CASE(R8_HL_DREF)
      PRINT_ENUM_CASE(R8_A)
    }
    return;
  case R16:
  case R16_MEM:
  case IMM16: fprintf(stream, " 0x%.4x", inst_param.imm16); return;
  case IMM16_MEM: fprintf(stream, " [0x%.4x]", inst_param.imm16); return;
  case UNKNOWN_INST_BYTE:
    fprintf(stream, " 0x%.2x", inst_param.unknown_inst_byte);
    return;
  case VOID_PARAM_TYPE: return;
  }
}

static void print_inst(FILE *stream, const struct inst inst) {
  switch (inst.type) {
  case NOP: fprintf(stream, "NOP"); break;
  case LD: fprintf(stream, "LD"); break;
  case JP: fprintf(stream, "JP"); break;
  case UNKNOWN_INST: fprintf(stream, "UNKNOWN"); break;
  }
  print_inst_param(stream, inst.p1);
  print_inst_param(stream, inst.p2);

  fprintf(stream, "\n");
}

// copies rom to the start of memory and start disassembly at 0x100 since the
// boot rom goes before that.
void disassemble_rom(FILE *stream, const uint8_t *rom_bytes,
                     const int rom_bytes_len) {
  struct gb_state gb_state;
  gb_state_init(&gb_state);
  memcpy(gb_state.rom0, rom_bytes, rom_bytes_len);

  while (gb_state.regs.pc < rom_bytes_len) {
    fprintf(stream, "0x%.4x: ", gb_state.regs.pc);
    struct inst inst = fetch(&gb_state);
    print_inst(stream, inst);
  }
}
// copies rom to the start of memory and start disassembly at 0x0 since we're
// just looking at 1 section.
void disassemble_section(FILE *stream, const uint8_t *section_bytes,
                         const int section_bytes_len) {
  struct gb_state gb_state;
  gb_state_init(&gb_state);
  gb_state.regs.pc = 0;
  memcpy(gb_state.rom0, section_bytes, section_bytes_len);

  while (gb_state.regs.pc < section_bytes_len) {
    fprintf(stream, "0x%.4x: ", gb_state.regs.pc);
    struct inst inst = fetch(&gb_state);
    print_inst(stream, inst);
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

/*
 *** This below test data corresponds to this portion of the SimpleSprite rom.
 * SimpleSprite:
 *   ; Shut down audio circuitry
 *   ld a, 0
 *   ld [rNR52], a
 *   call WaitForVBlank
 *
 *   call LCDOff
 *
 *   ld a, 16
 *   push af
 *
 *   ld hl, $9010
 *
 *   ld bc, DoggoSprite
 *
 *   call CopySprite
 *
 *   pop af
 *
 *   ; ClearMem - addr
 *   ld bc, _SCRN0
 *   push bc
 *   ; ClearMem - fill byte (f is just padding to keep stack 2 byte aligned)
 *   ld a, $00
 *   push af
 *   ; ClearMem - len
 *   ld bc, 32 * 32
 *   push bc
 *
 *   call ClearMem
 *   pop bc ; ClearMem - addr
 *   pop af ; ClearMem - fill byte
 *   pop bc ; ClearMem - len
 *
 *   ld hl, $9804
 *   ld [hl], 1
 *
 *   call LCDOn
 *
 *   ; During the first (blank) frame, initialize display registers
 *   ld a, %11100100
 *   ld [rBGP], a
 *
 *   call Done
 */
static const unsigned char _test_disasm_section[] = {
    0x3e, 0x00, 0xea, 0x26, 0xff, 0xcd, 0x89, 0x01, 0xcd, 0xb9, 0x01, 0x3e,
    0x10, 0xf5, 0x21, 0x10, 0x90, 0x01, 0xc8, 0x01, 0xcd, 0x92, 0x01, 0xf1,
    0x01, 0x00, 0x98, 0xc5, 0x3e, 0x00, 0xf5, 0x01, 0x00, 0x04, 0xc5, 0xcd,
    0x9e, 0x01, 0xc1, 0xf1, 0xc1, 0x21, 0x04, 0x98, 0x36, 0x01, 0xcd, 0xbf,
    0x01, 0x3e, 0xe4, 0xea, 0x47, 0xff, 0xcd, 0xc5, 0x01};
static const int _test_disasm_section_len = sizeof(_test_disasm_section);

static const char _test_expected_disasm_output[] = "0x0000: UNKNOWN 0x3e\n"
                                                   "0x0001: NOP\n"
                                                   "0x0002: UNKNOWN 0xea\n"
                                                   "0x0003: UNKNOWN 0x26\n"
                                                   "0x0004: UNKNOWN 0xff\n"
                                                   "0x0005: UNKNOWN 0xcd\n"
                                                   "0x0006: UNKNOWN 0x89\n"
                                                   "0x0007: LD 0x0000 0xb9cd\n"
                                                   "0x000a: LD 0x0000 0x103e\n"
                                                   "0x000d: UNKNOWN 0xf5\n"
                                                   "0x000e: LD 0x0002 0x9010\n"
                                                   "0x0011: LD 0x0000 0x01c8\n"
                                                   "0x0014: UNKNOWN 0xcd\n"
                                                   "0x0015: UNKNOWN 0x92\n"
                                                   "0x0016: LD 0x0000 0x01f1\n"
                                                   "0x0019: NOP\n"
                                                   "0x001a: UNKNOWN 0x98\n"
                                                   "0x001b: UNKNOWN 0xc5\n"
                                                   "0x001c: UNKNOWN 0x3e\n"
                                                   "0x001d: NOP\n"
                                                   "0x001e: UNKNOWN 0xf5\n"
                                                   "0x001f: LD 0x0000 0x0400\n"
                                                   "0x0022: UNKNOWN 0xc5\n"
                                                   "0x0023: UNKNOWN 0xcd\n"
                                                   "0x0024: UNKNOWN 0x9e\n"
                                                   "0x0025: LD 0x0000 0xf1c1\n"
                                                   "0x0028: UNKNOWN 0xc1\n"
                                                   "0x0029: LD 0x0002 0x9804\n"
                                                   "0x002c: UNKNOWN 0x36\n"
                                                   "0x002d: LD 0x0000 0xbfcd\n"
                                                   "0x0030: LD 0x0000 0xe43e\n"
                                                   "0x0033: UNKNOWN 0xea\n"
                                                   "0x0034: UNKNOWN 0x47\n"
                                                   "0x0035: UNKNOWN 0xff\n"
                                                   "0x0036: UNKNOWN 0xcd\n"
                                                   "0x0037: UNKNOWN 0xc5\n"
                                                   "0x0038: LD 0x0000 0x0000\n";
static const int _test_expected_disasm_output_len =
    sizeof(_test_expected_disasm_output);
void test_disasm() {
  FILE *stream = tmpfile();
  char buf[KB(10)];
  disassemble_section(stream, _test_disasm_section, _test_disasm_section_len);
  rewind(stream);
  int bytes_read = fread(buf, sizeof(*buf), sizeof(buf), stream);
  fprintf(stderr, "bytes read %d\n", bytes_read);
  assert(ferror(stream) == 0);
  assert(feof(stream) != 0);
  fclose(stream);
  if (_test_expected_disasm_output_len - 1 != bytes_read ||
      strncmp(buf, _test_expected_disasm_output, bytes_read) != 0) {
    fprintf(stderr, "text_disasm failed, expected:\n%s\nreceived:\n%.*s\n",
            _test_expected_disasm_output, bytes_read, buf);
    abort();
  }
}

void test_execute() { test_execute_load(); }

int main() {
  SDL_Log("Starting CPU tests.");
  SDL_Log("running `test_fetch()`");
  test_fetch();
  SDL_Log("running `test_execute()`");
  test_execute();
  SDL_Log("CPU tests succeeded.");
  SDL_Log("running `test_disasm()`");
  test_disasm();
  SDL_Log("CPU tests succeeded.");
  SDL_Quit();
}

#endif
