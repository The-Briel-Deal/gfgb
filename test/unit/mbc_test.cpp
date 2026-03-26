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
  REQUIRE(parsed_header.mbc_type == GB_MBC1);
}
