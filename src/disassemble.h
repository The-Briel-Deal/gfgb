#ifndef GB_DISASSEMBLE_H
#define GB_DISASSEMBLE_H

#include "cpu.h"

#include <stdint.h>
#include <stdio.h>

void disassemble(struct gb_state *gb_state, FILE *stream);

#define DBG_SYM_BOOTROM_BANK -1

// I'm treating sections and labels the same in the parsed data structure.
struct debug_symbol_list {
  struct debug_symbol {
    char name[16];
    int bank;
    uint16_t start_offset;
    uint16_t len;
  } *syms;
  uint16_t len;
  uint16_t capacity;
};
void alloc_symbol_list(struct debug_symbol_list *syms);
void free_symbol_list(struct debug_symbol_list *syms);

void parse_syms(struct debug_symbol_list *syms, FILE *sym_file);
void print_inst(FILE *stream, const struct inst inst);

#endif // GB_DISASSEMBLE_H
