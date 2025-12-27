
#include "common.h"
#include "cpu.h"

#define PRINT_ENUM_CASE(enum_case)                                             \
  case enum_case: sprintf(inst_param_str, "%s", #enum_case); break;

static void print_inst_param(char *inst_param_str,
                             const struct inst_param inst_param) {
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
    break;
  case R16:
    switch (inst_param.r16) {
      PRINT_ENUM_CASE(R16_BC)
      PRINT_ENUM_CASE(R16_DE)
      PRINT_ENUM_CASE(R16_HL)
      PRINT_ENUM_CASE(R16_SP)
    }
    break;
  case R16_MEM:
    switch (inst_param.r16_mem) {
      PRINT_ENUM_CASE(R16_MEM_BC)
      PRINT_ENUM_CASE(R16_MEM_DE)
      PRINT_ENUM_CASE(R16_MEM_HLI)
      PRINT_ENUM_CASE(R16_MEM_HLD)
    }
    break;
  case R16_STK:
    switch (inst_param.r16_stk) {
      PRINT_ENUM_CASE(R16_STK_BC)
      PRINT_ENUM_CASE(R16_STK_DE)
      PRINT_ENUM_CASE(R16_STK_HL)
      PRINT_ENUM_CASE(R16_STK_AF)
    }
    break;
  case IMM8: sprintf(inst_param_str, "0x%.2X", inst_param.imm8); break;
  case IMM16: sprintf(inst_param_str, "0x%.4X", inst_param.imm16); break;
  case IMM16_MEM: sprintf(inst_param_str, "[0x%.4X]", inst_param.imm16); break;
  case UNKNOWN_INST_BYTE:
    sprintf(inst_param_str, "0x%.2X", inst_param.unknown_inst_byte);
    break;
  case VOID_PARAM_TYPE: sprintf(inst_param_str, "(void)"); break;
  }
}
#undef PRINT_ENUM_CASE

#define PRINT_INST_NAME(stream, inst_name)                                     \
  case inst_name: fprintf(stream, "%-10s", #inst_name); break;

static void print_inst(FILE *stream, const struct inst inst) {
  switch (inst.type) {
    PRINT_INST_NAME(stream, NOP)
    PRINT_INST_NAME(stream, LD)
    PRINT_INST_NAME(stream, JP)
    PRINT_INST_NAME(stream, CALL)
    PRINT_INST_NAME(stream, POP)
    PRINT_INST_NAME(stream, PUSH)
    PRINT_INST_NAME(stream, UNKNOWN_INST)
  }
  char inst_param_str[16];
  print_inst_param(inst_param_str, inst.p1);
  fprintf(stream, "%-12s", inst_param_str);
  print_inst_param(inst_param_str, inst.p2);
  fprintf(stream, "%s\n", inst_param_str);
}

#undef PRINT_INST_NAME

// copies rom to the start of memory and start disassembly at 0x100 since the
// boot rom goes before that.
void disassemble_rom(FILE *stream, const uint8_t *rom_bytes,
                     const int rom_bytes_len) {
  struct gb_state gb_state;
  gb_state_init(&gb_state);
  memcpy(gb_state.rom0, rom_bytes, rom_bytes_len);

  while (gb_state.regs.pc < rom_bytes_len) {
    fprintf(stream, "0x%.4X: ", gb_state.regs.pc);
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
    fprintf(stream, "0x%.4X: ", gb_state.regs.pc);
    struct inst inst = fetch(&gb_state);
    print_inst(stream, inst);
  }
}
