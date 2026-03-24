#include "disassemble.h"
#include "common.h"
#include "cpu.h"
#include "tracy/Tracy.hpp"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PRINT_ENUM_CASE(enum_case)                                                                                     \
  case enum_case: sprintf(inst_param_str, "%s", #enum_case); break;

static void print_inst_param(char *inst_param_str, const struct inst_param inst_param) {
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
  case E8: sprintf(inst_param_str, "%d", *(int8_t *)&inst_param.imm8); break;
  case IMM8_HMEM: sprintf(inst_param_str, "[0x%.2X]", inst_param.imm8); break;
  case SP_IMM8: sprintf(inst_param_str, "SP+0x%.2X", inst_param.imm8); break;
  // TODO: Print label for imm16 when possible.
  case IMM16: sprintf(inst_param_str, "0x%.4X", inst_param.imm16); break;
  case IMM16_MEM: sprintf(inst_param_str, "[0x%.4X]", inst_param.imm16); break;
  case B3: sprintf(inst_param_str, "B3_%d", inst_param.b3); break;
  case TGT3: sprintf(inst_param_str, "TGT3_%d", inst_param.tgt3); break;
  case COND:
    switch (inst_param.cond) {
      PRINT_ENUM_CASE(COND_NZ)
      PRINT_ENUM_CASE(COND_Z)
      PRINT_ENUM_CASE(COND_NC)
      PRINT_ENUM_CASE(COND_C)
    }
    break;
  case UNKNOWN_INST_BYTE: sprintf(inst_param_str, "0x%.2X", inst_param.unknown_inst_byte); break;
  case VOID_PARAM_TYPE: sprintf(inst_param_str, "(void)"); break;
  }
}
#undef PRINT_ENUM_CASE

const debug_symbol_t *symbol_from_addr(const debug_symbol_list_t *syms, uint16_t addr) {
  // This works because we know the symbols are sorted.
  for (int i = 0; i < syms->len; i++) {
    debug_symbol_t *sym = &syms->syms[i];
    if (sym->start_offset <= addr && addr < sym->len + sym->start_offset) {
      return sym;
    }
  }
  return NULL;
}

#define PRINT_INST_NAME(stream, inst_name)                                                                             \
  case inst_name: {                                                                                                    \
    fprintf(stream, "%-10s", #inst_name);                                                                              \
    break;                                                                                                             \
  }

void print_inst(gb_state_t *gb_state, FILE *stream, const struct inst inst, bool show_inst_addr, uint16_t inst_addr) {
  ZoneScopedN("Print Inst");
  if (show_inst_addr) {
    const debug_symbol_t *sym = symbol_from_addr(&gb_state->dbg.syms, inst_addr);
    const char           *sym_name;
    if (sym != NULL) {
      sym_name = sym->name;
    } else {
      sym_name = "Unknown";
    }
    fprintf(stream, "%s:0x%.4X: ", sym_name, inst_addr);
  }
  switch (inst.type) {
    PRINT_INST_NAME(stream, ADC)
    PRINT_INST_NAME(stream, ADD)
    PRINT_INST_NAME(stream, AND)
    PRINT_INST_NAME(stream, BIT)
    PRINT_INST_NAME(stream, CALL)
    PRINT_INST_NAME(stream, CCF)
    PRINT_INST_NAME(stream, CP)
    PRINT_INST_NAME(stream, CPL)
    PRINT_INST_NAME(stream, DAA)
    PRINT_INST_NAME(stream, DEC)
    PRINT_INST_NAME(stream, DI)
    PRINT_INST_NAME(stream, EI)
    PRINT_INST_NAME(stream, HALT)
    PRINT_INST_NAME(stream, INC)
    PRINT_INST_NAME(stream, JP)
    PRINT_INST_NAME(stream, JR)
    PRINT_INST_NAME(stream, LD)
    PRINT_INST_NAME(stream, LDH)
    PRINT_INST_NAME(stream, NOP)
    PRINT_INST_NAME(stream, OR)
    PRINT_INST_NAME(stream, POP)
    PRINT_INST_NAME(stream, PUSH)
    PRINT_INST_NAME(stream, RES)
    PRINT_INST_NAME(stream, RET)
    PRINT_INST_NAME(stream, RETI)
    PRINT_INST_NAME(stream, RL)
    PRINT_INST_NAME(stream, RLA)
    PRINT_INST_NAME(stream, RLC)
    PRINT_INST_NAME(stream, RLCA)
    PRINT_INST_NAME(stream, RR)
    PRINT_INST_NAME(stream, RRA)
    PRINT_INST_NAME(stream, RRC)
    PRINT_INST_NAME(stream, RRCA)
    PRINT_INST_NAME(stream, RST)
    PRINT_INST_NAME(stream, SBC)
    PRINT_INST_NAME(stream, SCF)
    PRINT_INST_NAME(stream, SET)
    PRINT_INST_NAME(stream, SLA)
    PRINT_INST_NAME(stream, SRA)
    PRINT_INST_NAME(stream, SRL)
    PRINT_INST_NAME(stream, STOP)
    PRINT_INST_NAME(stream, SUB)
    PRINT_INST_NAME(stream, SWAP)
    PRINT_INST_NAME(stream, XOR)

  case UNKNOWN_INST: {
    // I only use the `_INST` suffix to prevent name collision, so i'm going
    // just print `UNKNOWN` here so I don't need to add more padding.
    fprintf(stream, "%-10s", "UNKNOWN");
    break;
  }
  }
  char inst_param_str[16];
  print_inst_param(inst_param_str, inst.p1);
  fprintf(stream, "%-12s", inst_param_str);
  print_inst_param(inst_param_str, inst.p2);
  fprintf(stream, "%s\n", inst_param_str);
}

#undef PRINT_INST_NAME

void alloc_symbol_list(struct debug_symbol_list *syms) {
  syms->len      = 0;
  syms->capacity = 12;
  syms->syms     = (debug_symbol_t *)GB_malloc(syms->capacity * sizeof(*syms->syms));
}

// TODO: Shrink array if len is half of capacity.
void realloc_symbol_list(struct debug_symbol_list *syms) {
  if (syms->len + 1 >= syms->capacity) {
    syms->capacity *= 2;
    syms->syms = (debug_symbol_t *)GB_realloc(syms->syms, sizeof(*syms->syms) * syms->capacity);
  }
}

void free_symbol_list(struct debug_symbol_list *syms) {
  GB_assert(syms->capacity != 0);
  free(syms->syms);
  syms->capacity = 0;
  syms->len      = 0;
}

const debug_symbol_t *symbol_from_name(const debug_symbol_list_t *syms, const char *name) {
  for (int i = 0; i < syms->len; i++) {
    if (strcmp(syms->syms[i].name, name) == 0) return &syms->syms[i];
  }
  return NULL;
}

void sort_syms(struct debug_symbol_list *syms) {
  // This is just bubble sort, since this is just for debugging it should be
  // fine. If it becomes an issue I could use something faster.
  struct debug_symbol tmp_sym;
  bool                swapped;
  int                 n = syms->len;

  for (int i = 0; i < n; i++) {
    swapped = false;
    for (int j = 0; j < (n - i - 1); j++) {
      // TODO: currently i'm just sorting by addr, once bank switching exists we
      // should also sort by bank.
      if (syms->syms[j].start_offset > syms->syms[j + 1].start_offset) {
        tmp_sym           = syms->syms[j];
        syms->syms[j]     = syms->syms[j + 1];
        syms->syms[j + 1] = tmp_sym;
        swapped           = true;
      }
    }
    if (!swapped) break;
  }
}

void set_sym_lens(struct debug_symbol_list *syms) {
  struct debug_symbol *curr_sym;
  struct debug_symbol *next_sym;
  int                  n = syms->len;
  for (int i = 0; i < n; i++) {
    curr_sym = &syms->syms[i];

    // Only set syms length if it is not the last sym.
    curr_sym->len = 0;
    if (i + 1 < n) {
      next_sym      = &syms->syms[i + 1];
      curr_sym->len = next_sym->start_offset - curr_sym->start_offset;
    }
  }
}

void parse_syms(struct debug_symbol_list *syms, FILE *sym_file) {
  char  line[KB(1)];
  char *ret;
  while (!feof(sym_file)) {
    realloc_symbol_list(syms);
    ret = fgets(line, sizeof(line), sym_file);
    if (ret == NULL) {
      if (ferror(sym_file) != 0) {
        return;
      }
      continue;
    }
    if (line[0] == ';') continue;
    char                *endptr;
    struct debug_symbol *curr_sym = &syms->syms[syms->len];

    if (line[0] == 'B') {
      GB_assert(line[1] == 'O' && line[2] == 'O' && line[3] == 'T' && line[4] == ':');
      endptr         = &line[4];
      curr_sym->bank = DBG_SYM_BOOTROM_BANK;
    } else {
      curr_sym->bank = strtol(&line[0], &endptr, 16);
      GB_assert(endptr == &line[2]);
    }
    char *bank_endptr = endptr;

    curr_sym->start_offset = strtol(bank_endptr + 1, &endptr, 16);
    GB_assert(endptr == bank_endptr + 5);

    strncpy(syms->syms[syms->len].name, endptr + 1, sizeof(syms->syms[syms->len].name) - 1);
    // In case the string is longer than the sym.name arr
    syms->syms[syms->len].name[sizeof(syms->syms[syms->len].name) - 1] = '\0';
    // Probably not the best way to do this but, I need this to be null
    // terminated instead of newline terminated.
    for (uint32_t i = 0; i < sizeof(syms->syms[syms->len].name); i++) {
      if (syms->syms[syms->len].name[i] == '\n') {
        syms->syms[syms->len].name[i] = '\0';
      }
      if (syms->syms[syms->len].name[i] == '\0') {
        break;
      }
    }

    syms->len++;
    GB_assert(syms->len < syms->capacity);
  }
  sort_syms(syms);
  set_sym_lens(syms);
}

// copies rom to the start of memory and start disassembly at 0x100 since the
// boot rom goes before that.
static void disassemble_rom(struct gb_state *gb_state, FILE *stream) {
  while (gb_state->saved.regs.pc < sizeof(gb_state->saved.ram.rom0)) {
    uint16_t    inst_addr = gb_state->saved.regs.pc;
    struct inst inst      = fetch(gb_state);
    fprintf(stream, "  ");
    print_inst(gb_state, stream, inst, true, inst_addr);
  }
}
static void disassemble_bootrom(struct gb_state *gb_state, FILE *stream) {
  gb_state->saved.regs.pc = 0x0000;
  while (gb_state->saved.regs.pc < 0x0100) {
    uint16_t    inst_addr = gb_state->saved.regs.pc;
    struct inst inst      = fetch(gb_state);
    fprintf(stream, "  ");
    print_inst(gb_state, stream, inst, true, inst_addr);
  }
}

// copies rom to the start of memory and start disassembly at 0x100 since the
// boot rom goes before that.
static void disassemble_rom_with_sym(struct gb_state *gb_state, FILE *stream) {
  const struct debug_symbol *curr_sym;
  for (int i = 0; i < gb_state->dbg.syms.len; i++) {
    curr_sym = &gb_state->dbg.syms.syms[i];
    fprintf(stream, "  %s:\n", curr_sym->name);
    gb_state->saved.regs.pc = curr_sym->start_offset;
    while (gb_state->saved.regs.pc < curr_sym->start_offset + curr_sym->len) {
      uint16_t    inst_addr = gb_state->saved.regs.pc;
      struct inst inst      = fetch(gb_state);
      fprintf(stream, "    ");
      print_inst(gb_state, stream, inst, true, inst_addr);
    }
  }
}

void disassemble(struct gb_state *gb_state, FILE *stream) {
  // TODO: I don't love how i'm doing this. Disassembly should probably be reworked to always start at 0 and just use
  // symbols where available.
  //
  // If bootrom has syms then they will be used in `disassemble_rom_with_sym()`
  if (gb_state->saved.regs.io.bank && !gb_state->dbg.bootrom_has_syms) {
    fprintf(stream, "BootRom:\n");
    disassemble_bootrom(gb_state, stream);
  }

  if (gb_state->dbg.rom_loaded) {
    fprintf(stream, "RomStart:\n");
    if (gb_state->dbg.syms.capacity > 0) {
      disassemble_rom_with_sym(gb_state, stream);
    } else {
      disassemble_rom(gb_state, stream);
    }
  }
}
