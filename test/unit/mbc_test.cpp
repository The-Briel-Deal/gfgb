#include "common.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

#define MBC1_ROM_512KB_PATH "test/mooneye/emulator-only/mbc1/rom_512kb.gb"
TEST_CASE("Parse MBC1 512kb ROM Header", "[mbc]") {
  FILE *f = fopen(MBC1_ROM_512KB_PATH, "r");
  CHECKED_IF(f == NULL) {
    INFO("Could not open '" << MBC1_ROM_512KB_PATH << "': " << strerror(errno) << ". Are you in the repository root?");
    FAIL();
  }
  REQUIRE(ferror(f) == 0);
  fseek(f, 0x100, SEEK_SET);
  uint8_t header[0x50];
  fread(header, 1, 0x50, f);
  gb_cart_header_t parsed_header = gb_parse_cart_header(header);
  CHECK(parsed_header.mbc_type == GB_MBC1);
  CHECK(parsed_header.has_battery == false);
  CHECK(parsed_header.has_ram == false);
  CHECK(parsed_header.has_rumble == false);
  CHECK(parsed_header.has_rtc == false);
  CHECK(parsed_header.num_rom_banks == 4);
  CHECK(parsed_header.num_ram_banks == 0);
}

TEST_CASE("Write to MBC1 regs", "[mbc]") {
  gb_state_t gb_state;
  gb_state_init(&gb_state);
  gb_load_rom(&gb_state, MBC1_ROM_512KB_PATH, NULL, NULL);
  mbc1_regs_t &mbc1_regs = gb_state.saved.mem.mbc.mbc1_regs;
  REQUIRE(gb_state.saved.header.mbc_type == GB_MBC1);

  /// RAM Enable
  // Reg should only be true if lower 4 are 0xA.

  // I'm also just validating that the rom is not being modified by making sure the value doesn't change from
  // gb_write_mem().
  gb_state.saved.mem.mbc.rom_start[0x12A1] = 0x4B;
  gb_state.saved.mem.mbc.rom_start[0x0000] = 0x4C;
  gb_state.saved.mem.mbc.rom_start[0x1FFF] = 0x4D;

  {
    gb_write_mem(&gb_state, 0x12A1, 0xFF);
    CHECK_FALSE(mbc1_regs.ram_enable);
    gb_write_mem(&gb_state, 0x0000, 0xFA);
    CHECK(mbc1_regs.ram_enable);
    gb_write_mem(&gb_state, 0x1FFF, 0xAF);
    CHECK_FALSE(mbc1_regs.ram_enable);
  }

  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x12A1], 0x4B);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x0000], 0x4C);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x1FFF], 0x4D);

  /// ROM Bank Num
  gb_state.saved.mem.mbc.rom_start[0x2000] = 0x4B;
  gb_state.saved.mem.mbc.rom_start[0x3120] = 0x4C;
  gb_state.saved.mem.mbc.rom_start[0x3FFF] = 0x4D;

  {
    // If the ROM Bank Num reg is more than the number of banks then this reg is masked to the max number of bits.
    REQUIRE_BYTES_EQ(gb_state.saved.header.num_rom_banks, 4);

    gb_write_mem(&gb_state, 0x2000, 0b1111'1111);
    CHECK_BYTES_EQ(mbc1_regs.rom_bank, 0b0000'0011);
    gb_write_mem(&gb_state, 0x3120, 0b1111'0101);
    CHECK_BYTES_EQ(mbc1_regs.rom_bank, 0b0000'0001);
    // If masked val is 0 it will be changed to 1.
    // See: https://gbdev.io/pandocs/MBC1.html#20003fff--rom-bank-number-write-only
    gb_write_mem(&gb_state, 0x3FFF, 0b0010'0000);
    CHECK_BYTES_EQ(mbc1_regs.rom_bank, 0b0000'0001);
  }

  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x2000], 0x4B);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x3120], 0x4C);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x3FFF], 0x4D);

  gb_state.saved.mem.mbc.rom_start[0x4000 + 0x0000] = 0x4B;
  gb_state.saved.mem.mbc.rom_start[0x4000 + 0x1000] = 0x4C;
  gb_state.saved.mem.mbc.rom_start[0x4000 + 0x1FFF] = 0x4D;
  {
    gb_write_mem(&gb_state, 0x5000, 0xFF);
    CHECK_BYTES_EQ(mbc1_regs.ram_bank, 3);
    gb_write_mem(&gb_state, 0x4000, 0xFE);
    CHECK_BYTES_EQ(mbc1_regs.ram_bank, 2);
    gb_write_mem(&gb_state, 0x5FFF, 0xF1);
    CHECK_BYTES_EQ(mbc1_regs.ram_bank, 1);
  }
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x4000 + 0x0000], 0x4B);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x4000 + 0x1000], 0x4C);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x4000 + 0x1FFF], 0x4D);

  gb_state.saved.mem.mbc.rom_start[0x4000 + 0x2000] = 0x4B;
  gb_state.saved.mem.mbc.rom_start[0x4000 + 0x3000] = 0x4C;
  gb_state.saved.mem.mbc.rom_start[0x4000 + 0x3FFF] = 0x4D;
  {
    gb_write_mem(&gb_state, 0x6000, 0xFF);
    CHECK_BYTES_EQ(mbc1_regs.banking_mode_select, 1);
    gb_write_mem(&gb_state, 0x7000, 0xFE);
    CHECK_BYTES_EQ(mbc1_regs.banking_mode_select, 0);
    gb_write_mem(&gb_state, 0x7FFF, 0xF1);
    CHECK_BYTES_EQ(mbc1_regs.banking_mode_select, 1);
  }
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x4000 + 0x2000], 0x4B);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x4000 + 0x3000], 0x4C);
  CHECK_BYTES_EQ(gb_state.saved.mem.mbc.rom_start[0x4000 + 0x3FFF], 0x4D);
}
