#ifndef GB_DISASSEMBLE_H
#define GB_DISASSEMBLE_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;
typedef struct gb_state gb_state_t;

// Disassemble entire loaded rom into stream.
void disassemble(gb_state_t *gb_state, FILE *stream);

#define DBG_SYM_BOOTROM_BANK -1

typedef struct debug_symbol {
  char     name[32];
  int      bank;
  uint16_t start_offset;
  uint16_t len;
} debug_symbol_t;

// I'm treating sections and labels the same in the parsed data structure.
typedef struct debug_symbol_list {
  debug_symbol_t *syms;
  uint16_t        len;
  uint16_t        capacity;
} debug_symbol_list_t;
void alloc_symbol_list(debug_symbol_list_t *syms);
void free_symbol_list(debug_symbol_list_t *syms);

// returns NULL if there is no symbol with provided name, otherwise returns a pointer to the corresponding debug_symbol.
// NOTE: This pointer may be invalidated if any new symbols are added since they may be moved on re-alloc. Don't hold
// onto this reference.
const debug_symbol_t *symbol_from_name(const debug_symbol_list_t *syms, const char *name);
// TODO: This will need to change to also take the mem bank once I add support for banking
const debug_symbol_t *symbol_from_addr(const debug_symbol_list_t *syms, uint16_t addr);

void parse_syms(debug_symbol_list_t *syms, FILE *sym_file);
void print_inst(gb_state_t *gb_state, FILE *stream, const struct inst inst, bool show_inst_addr, uint16_t inst_addr);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // GB_DISASSEMBLE_H
