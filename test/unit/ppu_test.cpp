#include "common.h"
#include "test_common.h"

void test_gb_tile_to_8bit_indexed() {
  uint8_t gb_tile_in[16]                  = {0};
  uint8_t indexed_8bit_tile_expect[8 * 8] = {0};
  uint8_t indexed_8bit_tile_result[8 * 8] = {0};
  gb_tile_in[0]                           = 0b1010'1100;
  gb_tile_in[1]                           = 0b1100'1011;
  indexed_8bit_tile_expect[0]             = 0b11;
  indexed_8bit_tile_expect[1]             = 0b10;
  indexed_8bit_tile_expect[2]             = 0b01;
  indexed_8bit_tile_expect[3]             = 0b00;
  indexed_8bit_tile_expect[4]             = 0b11;
  indexed_8bit_tile_expect[5]             = 0b01;
  indexed_8bit_tile_expect[6]             = 0b10;
  indexed_8bit_tile_expect[7]             = 0b10;

  gb_tile_to_8bit_indexed(gb_tile_in, indexed_8bit_tile_result);
  for (int i = 0; i < 16; i++) {
    if (indexed_8bit_tile_expect[i] != indexed_8bit_tile_result[i]) {
      LogInfo("byte i=%d of indexed_8bit result (%.2x) is not equal to result (%.2x)\n", i, indexed_8bit_tile_result[i],
              indexed_8bit_tile_expect[i]);
      abort();
    }
  }
}
int main() {
  LogInfo("Starting PPU tests.");
  LogInfo("running `test_gb_tile_to_indexed_8bit()`");
  test_gb_tile_to_8bit_indexed();
  LogInfo("PPU tests succeeded.");
  SDL_Quit();
}
