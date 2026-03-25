#include "common.h"
#include "test_common.h"

#include <array>

TEST_CASE("gb_tile_to_8bit_indexed()", "[ppu]") {
  uint8_t                      gb_tile_in[16]           = {0};
  std::array<std::byte, 8 * 8> indexed_8bit_tile_expect = {};
  std::array<std::byte, 8 * 8> indexed_8bit_tile_result = {};
  gb_tile_in[0]                                         = 0b1010'1100;
  gb_tile_in[1]                                         = 0b1100'1011;
  indexed_8bit_tile_expect[0]                           = std::byte(0b11);
  indexed_8bit_tile_expect[1]                           = std::byte(0b10);
  indexed_8bit_tile_expect[2]                           = std::byte(0b01);
  indexed_8bit_tile_expect[3]                           = std::byte(0b00);
  indexed_8bit_tile_expect[4]                           = std::byte(0b11);
  indexed_8bit_tile_expect[5]                           = std::byte(0b01);
  indexed_8bit_tile_expect[6]                           = std::byte(0b10);
  indexed_8bit_tile_expect[7]                           = std::byte(0b10);

  gb_tile_to_8bit_indexed(gb_tile_in, (uint8_t *)indexed_8bit_tile_result.data());
  REQUIRE(indexed_8bit_tile_result == indexed_8bit_tile_expect);
}
