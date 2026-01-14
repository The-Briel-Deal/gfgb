#include "disassemble.h"
#include "common.h"
#include "cpu.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
  case IMM8_HMEM: sprintf(inst_param_str, "[0x%.2X]", inst_param.imm8); break;
  case SP_IMM8: sprintf(inst_param_str, "SP+0x%.2X", inst_param.imm8); break;
  // TODO: Print label for imm16 when possible.
  case IMM16: sprintf(inst_param_str, "0x%.4X", inst_param.imm16); break;
  case IMM16_MEM: sprintf(inst_param_str, "[0x%.4X]", inst_param.imm16); break;
  case B3: sprintf(inst_param_str, "B3_%d", inst_param.b3); break;
  case COND:
    switch (inst_param.cond) {
      PRINT_ENUM_CASE(COND_NZ)
      PRINT_ENUM_CASE(COND_Z)
      PRINT_ENUM_CASE(COND_NC)
      PRINT_ENUM_CASE(COND_C)
    }
    break;
  case UNKNOWN_INST_BYTE:
    sprintf(inst_param_str, "0x%.2X", inst_param.unknown_inst_byte);
    break;
  case VOID_PARAM_TYPE: sprintf(inst_param_str, "(void)"); break;
  }
}
#undef PRINT_ENUM_CASE

#define PRINT_INST_NAME(stream, inst_name)                                     \
  case inst_name: {                                                            \
    fprintf(stream, "%-10s", #inst_name);                                      \
    break;                                                                     \
  }

void print_inst(FILE *stream, const struct inst inst) {
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
    PRINT_INST_NAME(stream, RL)
    PRINT_INST_NAME(stream, RLA)
    PRINT_INST_NAME(stream, RLC)
    PRINT_INST_NAME(stream, RLCA)
    PRINT_INST_NAME(stream, RR)
    PRINT_INST_NAME(stream, RRA)
    PRINT_INST_NAME(stream, RRC)
    PRINT_INST_NAME(stream, RRCA)
    PRINT_INST_NAME(stream, SBC)
    PRINT_INST_NAME(stream, SCF)
    PRINT_INST_NAME(stream, SET)
    PRINT_INST_NAME(stream, SLA)
    PRINT_INST_NAME(stream, SRA)
    PRINT_INST_NAME(stream, SRL)
    PRINT_INST_NAME(stream, SUB)
    PRINT_INST_NAME(stream, SWAP)
    PRINT_INST_NAME(stream, XOR)

  case UNKNOWN_INST: {
    // I only use the `_INST` suffix to prevent name collision, so i'm going
    // just print `UNKNOWN` here so I don't need to add more padding.
    fprintf(stream, "%-10s", "UNKNOWN");
    break;
  }
  default:
    printf("Unhandled instruction in disassembler, aborting.\n");
    abort();
  }
  char inst_param_str[16];
  print_inst_param(inst_param_str, inst.p1);
  fprintf(stream, "%-12s", inst_param_str);
  print_inst_param(inst_param_str, inst.p2);
  fprintf(stream, "%s\n", inst_param_str);
}

#undef PRINT_INST_NAME

void alloc_symbol_list(struct debug_symbol_list *syms) {
  syms->len = 0;
  syms->capacity = 12;
  syms->syms = calloc(syms->capacity, sizeof(*syms->syms));
}

// TODO: Shrink array if len is half of capacity.
void realloc_symbol_list(struct debug_symbol_list *syms) {
  if (syms->len + 1 >= syms->capacity) {
    syms->capacity *= 2;
    syms->syms = realloc(syms->syms, sizeof(*syms->syms) * syms->capacity);
  }
}

void free_symbol_list(struct debug_symbol_list *syms) {
  assert(syms->capacity != 0);
  free(syms->syms);
  syms->capacity = 0;
  syms->len = 0;
}

void sort_syms(struct debug_symbol_list *syms) {
  // This is just bubble sort, since this is just for debugging it should be
  // fine. If it becomes an issue I could use something faster.
  struct debug_symbol tmp_sym;
  bool swapped;
  int n = syms->len;

  for (int i = 0; i < n; i++) {
    swapped = false;
    for (int j = 0; j < (n - i - 1); j++) {
      // TODO: currently i'm just sorting by addr, once bank switching exists we
      // should also sort by bank.
      if (syms->syms[j].start_offset > syms->syms[j + 1].start_offset) {
        tmp_sym = syms->syms[j];
        syms->syms[j] = syms->syms[j + 1];
        syms->syms[j + 1] = tmp_sym;
        swapped = true;
      }
    }
    if (swapped) break;
  }
}

void parse_syms(struct debug_symbol_list *syms, FILE *sym_file) {
  char line[KB(1)];
  char *ret;
  while (!feof(sym_file)) {
    realloc_symbol_list(syms);
    ret = fgets(line, sizeof(line), sym_file);
    if (ret == NULL) {
      if (ferror(sym_file) != 0) return;
      continue;
    }
    if (line[0] == ';') continue;
    char *endptr;
    struct debug_symbol *curr_sym = &syms->syms[syms->len];

    if (line[0] == 'B') {
      assert(line[1] == 'O' && line[2] == 'O' && line[3] == 'T' &&
             line[4] == ':');
      endptr = &line[4];
      curr_sym->bank = DBG_SYM_BOOTROM_BANK;
    } else {
      curr_sym->bank = strtol(&line[0], &endptr, 16);
      assert(endptr == &line[2]);
    }
    char *bank_endptr = endptr;

    curr_sym->start_offset = strtol(bank_endptr + 1, &endptr, 16);
    assert(endptr == bank_endptr + 5);

    curr_sym->len = 0;

    if (syms->len > 0) {
      struct debug_symbol *prev_sym = &syms->syms[syms->len - 1];
      prev_sym->len = curr_sym->start_offset - prev_sym->start_offset;
    }

    strncpy(syms->syms[syms->len].name, &line[8],
            sizeof(syms->syms[syms->len].name) - 1);
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
    // TODO: dynamically grow beyond starting capacity.
    assert(syms->len < syms->capacity);
  }
}

// copies rom to the start of memory and start disassembly at 0x100 since the
// boot rom goes before that.
static void disassemble_rom(struct gb_state *gb_state, FILE *stream) {

  while (gb_state->regs.pc < sizeof(gb_state->rom0)) {
    fprintf(stream, "  0x%.4X: ", gb_state->regs.pc);
    struct inst inst = fetch(gb_state);
    print_inst(stream, inst);
  }
}
static void disassemble_bootrom(struct gb_state *gb_state, FILE *stream) {
  gb_state->regs.pc = 0x0000;
  while (gb_state->regs.pc < 0x0100) {
    fprintf(stream, "  0x%.4X: ", gb_state->regs.pc);
    struct inst inst = fetch(gb_state);
    print_inst(stream, inst);
  }
}

// copies rom to the start of memory and start disassembly at 0x100 since the
// boot rom goes before that.
static void disassemble_rom_with_sym(struct gb_state *gb_state, FILE *stream) {

  const struct debug_symbol *curr_sym;
  for (int i = 0; i < gb_state->syms.len; i++) {
    curr_sym = &gb_state->syms.syms[i];
    fprintf(stream, "  %s:\n", curr_sym->name);
    gb_state->regs.pc = curr_sym->start_offset;
    while (gb_state->regs.pc < curr_sym->start_offset + curr_sym->len) {
      fprintf(stream, "    0x%.4X: ", gb_state->regs.pc);
      struct inst inst = fetch(gb_state);
      print_inst(stream, inst);
    }
  }
}
// copies rom to the start of memory and start disassembly at 0x0 since we're
// just looking at 1 section.
static void disassemble_section(FILE *stream, const uint8_t *section_bytes,
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

void disassemble(struct gb_state *gb_state, FILE *stream) {
  if (gb_state->bootrom_mapped) {
    fprintf(stream, "BootRom:\n");
    disassemble_bootrom(gb_state, stream);
  }

  if (gb_state->rom_loaded) {
    fprintf(stream, "RomStart:\n");
    if (gb_state->syms.capacity > 0) {
      disassemble_rom_with_sym(gb_state, stream);
    } else {
      disassemble_rom(gb_state, stream);
    }
  }
}

#ifdef RUN_DISASSEMBLE_TESTS

#include "test_asserts.h"

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

static const char _test_expected_disasm_output[] =
    "0x0000: LD        R8_A        0x00\n"
    "0x0002: LD        [0xFF26]    R8_A\n"
    "0x0005: CALL      0x0189      (void)\n"
    "0x0008: CALL      0x01B9      (void)\n"
    "0x000B: LD        R8_A        0x10\n"
    "0x000D: PUSH      R16_STK_AF  (void)\n"
    "0x000E: LD        R16_HL      0x9010\n"
    "0x0011: LD        R16_BC      0x01C8\n"
    "0x0014: CALL      0x0192      (void)\n"
    "0x0017: POP       R16_STK_AF  (void)\n"
    "0x0018: LD        R16_BC      0x9800\n"
    "0x001B: PUSH      R16_STK_BC  (void)\n"
    "0x001C: LD        R8_A        0x00\n"
    "0x001E: PUSH      R16_STK_AF  (void)\n"
    "0x001F: LD        R16_BC      0x0400\n"
    "0x0022: PUSH      R16_STK_BC  (void)\n"
    "0x0023: CALL      0x019E      (void)\n"
    "0x0026: POP       R16_STK_BC  (void)\n"
    "0x0027: POP       R16_STK_AF  (void)\n"
    "0x0028: POP       R16_STK_BC  (void)\n"
    "0x0029: LD        R16_HL      0x9804\n"
    "0x002C: LD        R8_HL_DREF  0x01\n"
    "0x002E: CALL      0x01BF      (void)\n"
    "0x0031: LD        R8_A        0xE4\n"
    "0x0033: LD        [0xFF47]    R8_A\n"
    "0x0036: CALL      0x01C5      (void)\n";

static const int _test_expected_disasm_output_len =
    sizeof(_test_expected_disasm_output);

void test_disasm() {
  FILE *stream = tmpfile();
  char buf[KB(10)];
  disassemble_section(stream, _test_disasm_section, _test_disasm_section_len);
  rewind(stream);
  int bytes_read = fread(buf, sizeof(*buf), sizeof(buf), stream);
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

static const char _test_parse_debug_sym_input[] =
    "; File generated by rgblink\n"
    "00:0150 SimpleSprite\n"
    "00:0189 WaitForVBlank\n"
    "00:0192 CopySprite\n"
    "00:0197 CopySprite.loop\n"
    "00:019e ClearMem\n"
    "00:01af ClearMem.loop\n"
    "00:01b9 LCDOff\n"
    "00:01bf LCDOn\n"
    "00:01c5 ThisIsALongSymbolNameToTestTruncation\n"
    "00:01c8 DoggoSprite";

static const char _test_parse_bootrom_debug_sym_input[] =
    "; File generated by rgblink\n"
    "BOOT:0000 EntryPoint\n"
    "BOOT:0007 EntryPoint.clearVRAM\n"
    "BOOT:0027 EntryPoint.decompressLogo\n"
    "BOOT:0039 EntryPoint.copyRTile\n"
    "BOOT:0048 EntryPoint.writeTilemapRow\n"
    "BOOT:004a EntryPoint.writeTilemapByte\n"
    "BOOT:0055 ScrollLogo\n"
    "BOOT:0060 ScrollLogo.loop\n"
    "BOOT:0062 ScrollLogo.delayFrames\n"
    "BOOT:0064 ScrollLogo.waitVBlank\n"
    "BOOT:0080 ScrollLogo.playSound\n"
    "BOOT:0086 ScrollLogo.dontPlaySound\n"
    "BOOT:0095 DecompressFirstNibble\n"
    "BOOT:0096 DecompressSecondNibble\n"
    "BOOT:0098 DecompressSecondNibble.loop\n"
    "BOOT:00a8 Logo\n"
    "BOOT:00d8 RTile\n"
    "BOOT:00e0 CheckLogo\n"
    "BOOT:00e6 CheckLogo.compare\n"
    "BOOT:00e9 CheckLogo.logoFailure\n"
    "BOOT:00f4 CheckLogo.computeChecksum\n"
    "BOOT:00fa CheckLogo.checksumFailure\n"
    "BOOT:0104 HeaderLogo\n"
    "BOOT:0134 HeaderTitle\n"
    "BOOT:013f HeaderMenufacturer\n"
    "BOOT:0143 HeaderCGBCompat\n"
    "BOOT:0144 HeaderNewLicensee\n"
    "BOOT:0146 HeaderSGBFlag\n"
    "BOOT:0147 HeaderCartType\n"
    "BOOT:0148 HeaderROMSize\n"
    "BOOT:0149 HeaderRAMSize\n"
    "BOOT:014a HeaderRegionCode\n"
    "BOOT:014b HeaderOldLicensee\n"
    "BOOT:014c HeaderROMVersion\n"
    "BOOT:014d HeaderChecksum\n"
    "BOOT:014e HeaderGlobalChecksum\n"
    "00:8000 vBlankTile\n"
    "00:8010 vLogoTiles\n"
    "00:8190 vRTile\n"
    "00:9800 vMainTilemap\n"
    "00:fffe hStackBottom";

void test_parse_debug_sym() {
  struct debug_symbol_list syms;
  // Parse rom debug syms
  {
    FILE *stream = tmpfile();
    fwrite(_test_parse_debug_sym_input, sizeof(*_test_parse_debug_sym_input),
           sizeof(_test_parse_debug_sym_input), stream);
    fflush(stream);
    rewind(stream);

    alloc_symbol_list(&syms);
    parse_syms(&syms, stream);
    fclose(stream);
  }
  // Parse bootrom debug syms
  {
    FILE *stream = tmpfile();
    fwrite(_test_parse_bootrom_debug_sym_input,
           sizeof(*_test_parse_bootrom_debug_sym_input),
           sizeof(_test_parse_bootrom_debug_sym_input), stream);
    fflush(stream);
    rewind(stream);

    parse_syms(&syms, stream);
    fclose(stream);
  }

  assert_eq(syms.len, 51);

  assert_eq(syms.syms[0].bank, 0x00);
  assert_eq(syms.syms[0].start_offset, 0x0150);
  assert_eq(syms.syms[0].len, 0x0039);
  assert_eq(syms.syms[0].name, "SimpleSprite");

  assert_eq(syms.syms[1].bank, 0x00);
  assert_eq(syms.syms[1].start_offset, 0x0189);
  assert_eq(syms.syms[1].len, 0x0009);
  assert_eq(syms.syms[1].name, "WaitForVBlank");

  assert_eq(syms.syms[2].bank, 0x00);
  assert_eq(syms.syms[2].start_offset, 0x0192);
  assert_eq(syms.syms[2].len, 0x0005);
  assert_eq(syms.syms[2].name, "CopySprite");

  assert_eq(syms.syms[3].bank, 0x00);
  assert_eq(syms.syms[3].start_offset, 0x0197);
  assert_eq(syms.syms[3].len, 0x0007);
  assert_eq(syms.syms[3].name, "CopySprite.loop");

  assert_eq(syms.syms[4].bank, 0x00);
  assert_eq(syms.syms[4].start_offset, 0x019E);
  assert_eq(syms.syms[4].len, 0x0011);
  assert_eq(syms.syms[4].name, "ClearMem");

  assert_eq(syms.syms[5].bank, 0x00);
  assert_eq(syms.syms[5].start_offset, 0x01AF);
  assert_eq(syms.syms[5].len, 0x000A);
  assert_eq(syms.syms[5].name, "ClearMem.loop");

  assert_eq(syms.syms[6].bank, 0x00);
  assert_eq(syms.syms[6].start_offset, 0x01B9);
  assert_eq(syms.syms[6].len, 0x0006);
  assert_eq(syms.syms[6].name, "LCDOff");

  assert_eq(syms.syms[7].bank, 0x00);
  assert_eq(syms.syms[7].start_offset, 0x01BF);
  assert_eq(syms.syms[7].len, 0x0006);
  assert_eq(syms.syms[7].name, "LCDOn");

  assert_eq(syms.syms[8].bank, 0x00);
  assert_eq(syms.syms[8].start_offset, 0x01C5);
  assert_eq(syms.syms[8].len, 0x0003);
  // Truncated to 15 chars (the 16th is a null terminator).
  assert_eq(syms.syms[8].name, "ThisIsALongSymb");

  // Since this is the last symbol, it has an unknown len, due to this we leave
  // the length as 0.
  assert_eq(syms.syms[9].bank, 0x00);
  assert_eq(syms.syms[9].start_offset, 0x01C8);
  assert_eq(syms.syms[9].len, 0x0000);
  assert_eq(syms.syms[9].name, "DoggoSprite");

  free_symbol_list(&syms);
}

int main() {
  SDL_Log("Starting Disassemble tests.");
  SDL_Log("running `test_disasm()`");
  test_disasm();
  SDL_Log("running `test_parse_debug_sym()`");
  test_parse_debug_sym();
  SDL_Log("Disassemble tests succeeded.");
  SDL_Quit();
}

#endif
