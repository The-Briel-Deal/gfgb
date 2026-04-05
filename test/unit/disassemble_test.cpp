#include "common.h"
#include "test_common.h"

#include <sstream>

// copies rom to the start of memory and start disassembly at 0x0 since we're
// just looking at 1 section. This is currently only used in tests.
static void disassemble_section(FILE *stream, const uint8_t *section_bytes, const int section_bytes_len) {
  struct gb_state gb_state;
  gb_state.saved.header.mbc_type = GB_NO_MBC;
  gb_alloc_mbc(&gb_state);
  gb_state.saved.regs.pc = 0;
  memcpy(gb_state.saved.mem.rom_start, section_bytes, section_bytes_len);

  while (gb_state.saved.regs.pc < section_bytes_len) {
    uint16_t    inst_addr = gb_state.saved.regs.pc;
    struct inst inst      = fetch(&gb_state);
    print_inst(&gb_state, stream, inst, true, inst_addr);
  }
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
    0x3e, 0x00, 0xea, 0x26, 0xff, 0xcd, 0x89, 0x01, 0xcd, 0xb9, 0x01, 0x3e, 0x10, 0xf5, 0x21, 0x10, 0x90, 0x01, 0xc8,
    0x01, 0xcd, 0x92, 0x01, 0xf1, 0x01, 0x00, 0x98, 0xc5, 0x3e, 0x00, 0xf5, 0x01, 0x00, 0x04, 0xc5, 0xcd, 0x9e, 0x01,
    0xc1, 0xf1, 0xc1, 0x21, 0x04, 0x98, 0x36, 0x01, 0xcd, 0xbf, 0x01, 0x3e, 0xe4, 0xea, 0x47, 0xff, 0xcd, 0xc5, 0x01};
static const int _test_disasm_section_len = sizeof(_test_disasm_section);

static const char _test_expected_disasm_output[] = "Unknown:0x0000: LD        R8_A        0x00\n"
                                                   "Unknown:0x0002: LD        [0xFF26]    R8_A\n"
                                                   "Unknown:0x0005: CALL      0x0189      (void)\n"
                                                   "Unknown:0x0008: CALL      0x01B9      (void)\n"
                                                   "Unknown:0x000B: LD        R8_A        0x10\n"
                                                   "Unknown:0x000D: PUSH      R16_STK_AF  (void)\n"
                                                   "Unknown:0x000E: LD        R16_HL      0x9010\n"
                                                   "Unknown:0x0011: LD        R16_BC      0x01C8\n"
                                                   "Unknown:0x0014: CALL      0x0192      (void)\n"
                                                   "Unknown:0x0017: POP       R16_STK_AF  (void)\n"
                                                   "Unknown:0x0018: LD        R16_BC      0x9800\n"
                                                   "Unknown:0x001B: PUSH      R16_STK_BC  (void)\n"
                                                   "Unknown:0x001C: LD        R8_A        0x00\n"
                                                   "Unknown:0x001E: PUSH      R16_STK_AF  (void)\n"
                                                   "Unknown:0x001F: LD        R16_BC      0x0400\n"
                                                   "Unknown:0x0022: PUSH      R16_STK_BC  (void)\n"
                                                   "Unknown:0x0023: CALL      0x019E      (void)\n"
                                                   "Unknown:0x0026: POP       R16_STK_BC  (void)\n"
                                                   "Unknown:0x0027: POP       R16_STK_AF  (void)\n"
                                                   "Unknown:0x0028: POP       R16_STK_BC  (void)\n"
                                                   "Unknown:0x0029: LD        R16_HL      0x9804\n"
                                                   "Unknown:0x002C: LD        R8_HL_DREF  0x01\n"
                                                   "Unknown:0x002E: CALL      0x01BF      (void)\n"
                                                   "Unknown:0x0031: LD        R8_A        0xE4\n"
                                                   "Unknown:0x0033: LD        [0xFF47]    R8_A\n"
                                                   "Unknown:0x0036: CALL      0x01C5      (void)\n";

static const int _test_expected_disasm_output_len = sizeof(_test_expected_disasm_output);

TEST_CASE("Test Disassembly", "[disasm]") {
  FILE *stream = tmpfile();
  char  buf[KB(10)];
  disassemble_section(stream, _test_disasm_section, _test_disasm_section_len);
  rewind(stream);
  int bytes_read = fread(buf, sizeof(*buf), sizeof(buf), stream);
  GB_assert(ferror(stream) == 0);
  GB_assert(feof(stream) != 0);
  fclose(stream);
  if (_test_expected_disasm_output_len - 1 != bytes_read ||
      strncmp(buf, _test_expected_disasm_output, bytes_read) != 0) {
    fprintf(stderr, "text_disasm failed, expected:\n%s\nreceived:\n%.*s\n", _test_expected_disasm_output, bytes_read,
            buf);
    abort();
  }
}

static const char _test_parse_debug_sym_input[] = "; File generated by rgblink\n"
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

static const char _test_parse_bootrom_debug_sym_input[] = "; File generated by rgblink\n"
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

TEST_CASE("Parse No$GB Debug Symbols", "[disasm]") {
  gb_state_t           gb_state;
  debug_symbol_list_t &syms = gb_state.dbg.syms;
  // Parse rom debug syms
  {
    std::istringstream input_stream(_test_parse_debug_sym_input);
    gb_state.load_syms(input_stream);
  }
  // Parse bootrom debug syms
  {
    std::istringstream input_stream(_test_parse_bootrom_debug_sym_input);
    gb_state.load_syms(input_stream);
  }

  assert_eq(syms.len, 51);

#define TEST_SYM(idx, _bank, _start_offset, _len, _name)                                                               \
  {                                                                                                                    \
    assert_eq(syms.syms[idx].bank, _bank);                                                                             \
    assert_eq(syms.syms[idx].start_offset, _start_offset);                                                             \
    assert_eq(syms.syms[idx].len, _len);                                                                               \
    assert_eq(syms.syms[idx].name, _name);                                                                             \
  }

#define BR_BANK DBG_SYM_BOOTROM_BANK

  TEST_SYM(0, BR_BANK, 0x00, 0x07 - 0x00, "EntryPoint");
  TEST_SYM(1, BR_BANK, 0x07, 0x27 - 0x07, "EntryPoint.clearVRAM");
  TEST_SYM(2, BR_BANK, 0x27, 0x39 - 0x27, "EntryPoint.decompressLogo");
  TEST_SYM(3, BR_BANK, 0x39, 0x48 - 0x39, "EntryPoint.copyRTile");
  TEST_SYM(4, BR_BANK, 0x48, 0x4A - 0x48, "EntryPoint.writeTilemapRow");
  TEST_SYM(5, BR_BANK, 0x4A, 0x55 - 0x4A, "EntryPoint.writeTilemapByte");
  TEST_SYM(6, BR_BANK, 0x55, 0x60 - 0x55, "ScrollLogo");
  TEST_SYM(7, BR_BANK, 0x60, 0x62 - 0x60, "ScrollLogo.loop");
  TEST_SYM(8, BR_BANK, 0x62, 0x64 - 0x62, "ScrollLogo.delayFrames");
  TEST_SYM(9, BR_BANK, 0x64, 0x80 - 0x64, "ScrollLogo.waitVBlank");
  TEST_SYM(10, BR_BANK, 0x80, 0x86 - 0x80, "ScrollLogo.playSound");
  TEST_SYM(11, BR_BANK, 0x86, 0x95 - 0x86, "ScrollLogo.dontPlaySound");
  TEST_SYM(12, BR_BANK, 0x95, 0x96 - 0x95, "DecompressFirstNibble");
  TEST_SYM(13, BR_BANK, 0x96, 0x98 - 0x96, "DecompressSecondNibble");
  TEST_SYM(14, BR_BANK, 0x98, 0xa8 - 0x98, "DecompressSecondNibble.loop");
  TEST_SYM(15, BR_BANK, 0xa8, 0xd8 - 0xa8, "Logo");
  TEST_SYM(16, BR_BANK, 0xd8, 0xe0 - 0xd8, "RTile");
  TEST_SYM(17, BR_BANK, 0xe0, 0xe6 - 0xe0, "CheckLogo");
  TEST_SYM(18, BR_BANK, 0xe6, 0xe9 - 0xe6, "CheckLogo.compare");
  TEST_SYM(19, BR_BANK, 0xe9, 0xf4 - 0xe9, "CheckLogo.logoFailure");
  TEST_SYM(20, BR_BANK, 0xf4, 0xfa - 0xf4, "CheckLogo.computeChecksum");
  TEST_SYM(21, BR_BANK, 0xfa, 0x104 - 0xfa, "CheckLogo.checksumFailure");
  TEST_SYM(22, BR_BANK, 0x0104, 0x0134 - 0x0104, "HeaderLogo");
  TEST_SYM(23, BR_BANK, 0x0134, 0x013f - 0x0134, "HeaderTitle");
  TEST_SYM(24, BR_BANK, 0x013f, 0x0143 - 0x013f, "HeaderMenufacturer");
  TEST_SYM(25, BR_BANK, 0x0143, 0x0144 - 0x0143, "HeaderCGBCompat");
  TEST_SYM(26, BR_BANK, 0x0144, 0x0146 - 0x0144, "HeaderNewLicensee");
  TEST_SYM(27, BR_BANK, 0x0146, 0x0147 - 0x0146, "HeaderSGBFlag");
  TEST_SYM(28, BR_BANK, 0x0147, 0x0148 - 0x0147, "HeaderCartType");
  TEST_SYM(29, BR_BANK, 0x0148, 0x0149 - 0x0148, "HeaderROMSize");
  TEST_SYM(30, BR_BANK, 0x0149, 0x014a - 0x0149, "HeaderRAMSize");
  TEST_SYM(31, BR_BANK, 0x014a, 0x014b - 0x014a, "HeaderRegionCode");
  TEST_SYM(32, BR_BANK, 0x014b, 0x014c - 0x014b, "HeaderOldLicensee");
  TEST_SYM(33, BR_BANK, 0x014c, 0x014d - 0x014c, "HeaderROMVersion");
  TEST_SYM(34, BR_BANK, 0x014d, 0x014e - 0x014d, "HeaderChecksum");
  TEST_SYM(35, BR_BANK, 0x014e, 0x0150 - 0x014e, "HeaderGlobalChecksum");

  TEST_SYM(36, 0, 0x0150, 0x39, "SimpleSprite");
  TEST_SYM(37, 0, 0x0189, 0x09, "WaitForVBlank");
  TEST_SYM(38, 0, 0x0192, 0x05, "CopySprite");
  TEST_SYM(39, 0, 0x0197, 0x07, "CopySprite.loop");
  TEST_SYM(40, 0, 0x019E, 0x11, "ClearMem");
  TEST_SYM(41, 0, 0x01AF, 0x0A, "ClearMem.loop");
  TEST_SYM(42, 0, 0x01B9, 0x06, "LCDOff");
  TEST_SYM(43, 0, 0x01BF, 0x06, "LCDOn");
  TEST_SYM(44, 0, 0x01C5, 0x03, "ThisIsALongSymbolNameToTestTrun");
  TEST_SYM(45, 0, 0x01C8, 0x7E38, "DoggoSprite");

#undef TEST_SYM
#undef BR_BANK

  free_symbol_list(&syms);
}
