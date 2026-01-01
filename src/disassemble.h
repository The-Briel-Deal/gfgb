#ifndef GB_DISASSEMBLE_H
#define GB_DISASSEMBLE_H

#include "cpu.h"

#include <stdint.h>
#include <stdio.h>

void disassemble_rom(FILE *stream, const uint8_t *rom_bytes,
                     const int rom_bytes_len);
// I'm treating sections and labels the same in the parsed data structure.
struct debug_symbol_list {
  struct debug_symbol {
    char name[16];
    uint8_t bank;
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

void disassemble_rom_with_sym(FILE *stream, const uint8_t *rom_bytes,
                              const int rom_bytes_len,
                              const struct debug_symbol_list *syms);
void disassemble_section(FILE *stream, const uint8_t *section_bytes,
                         const int section_bytes_len);

#endif // GB_DISASSEMBLE_H
