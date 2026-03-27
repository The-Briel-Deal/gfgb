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
  CHECK(parsed_header.num_banks == 4);
  CHECK(parsed_header.ram_banks == 0);
}

TEST_CASE("Load MBC1 512kb ROM", "[mbc]") {
  gb_state_t gb_state;
  gb_state_init(&gb_state);
  gb_load_rom(&gb_state, MBC1_ROM_512KB_PATH, NULL, NULL);
}
