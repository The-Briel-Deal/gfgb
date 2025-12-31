#ifndef GB_DISASSEMBLE_H
#define GB_DISASSEMBLE_H

#include <stdint.h>
#include <stdio.h>

void disassemble_rom(FILE *stream, const uint8_t *rom_bytes,
                     const int rom_bytes_len);
void disassemble_section(FILE *stream, const uint8_t *section_bytes,
                         const int section_bytes_len);

#endif // GB_DISASSEMBLE_H
