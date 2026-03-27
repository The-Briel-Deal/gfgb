#include "common.h"
#include "test_common.h"

#include <stdio.h>
#include <string.h>

TEST_CASE("Parse MBC1 512kb ROM Header", "[mbc]") {
  const char *fpath = "test/mooneye/emulator-only/mbc1/rom_512kb.gb";
  FILE       *f     = fopen(fpath, "r");
  CHECKED_IF(f == NULL) {
    INFO("Could not open '" << fpath << "': " << strerror(errno) << ". Are you in the repository root?");
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
