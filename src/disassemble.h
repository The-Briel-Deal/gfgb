#ifndef GB_DISASSEMBLE_H
#define GB_DISASSEMBLE_H

#include "cpu.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
void disassemble(struct gb_state *gb_state, FILE *stream);

#define DBG_SYM_BOOTROM_BANK -1

struct debug_symbol {
  char     name[32];
  int      bank;
  uint16_t start_offset;
  uint16_t len;
};
typedef struct debug_symbol debug_symbol_t;

// I'm treating sections and labels the same in the parsed data structure.
struct debug_symbol_list {
  debug_symbol_t *syms;
  uint16_t        len;
  uint16_t        capacity;
};
void alloc_symbol_list(struct debug_symbol_list *syms);
void free_symbol_list(struct debug_symbol_list *syms);

void parse_syms(struct debug_symbol_list *syms, FILE *sym_file);
void print_inst(FILE *stream, const struct inst inst);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GB_DISASSEMBLE_H
