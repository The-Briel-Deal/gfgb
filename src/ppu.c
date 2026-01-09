#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <stdint.h>

void gb_tile_to_msb2(uint8_t *gb_tile_in, uint8_t *msb2_tile_out) {
  for (int i = 0; i < 8; i++) {
    uint8_t b1 = gb_tile_in[(i * 2) + 0];
    uint8_t b2 = gb_tile_in[(i * 2) + 1];
    msb2_tile_out[(i * 2) + 0] = 0;
    msb2_tile_out[(i * 2) + 1] = 0;
    for (int j = 0; j < 8; j++) {
      msb2_tile_out[(i * 2) + (j / 4)] |= ((1 << j) & b1);
      msb2_tile_out[(i * 2) + (j / 4)] |= (((1 << j) & b2) >> 1);
    }
  }
}
#ifdef RUN_PPU_TESTS

#include "test_asserts.h"

#include <assert.h>

void test_gb_tile_to_msb2() {
  uint8_t gb_tile_in[16] = {0};
  uint8_t msb2_tile_expect[16] = {0};
  uint8_t msb2_tile_result[16] = {0};
  gb_tile_in[0] = 0b1010'1100;
  gb_tile_in[1] = 0b1100'1011;
  msb2_tile_expect[0] = 0b1110'0100;
  msb2_tile_expect[1] = 0b1101'1010;

  gb_tile_to_msb2(gb_tile_in, msb2_tile_result);
  for (int i = 0; i < 16; i++) {
    if (msb2_tile_expect[i] != msb2_tile_result[i]) {
      SDL_Log("byte i=%d of msb2 result (%.8b) is not equal to result (%.8b)\n", i,
             msb2_tile_result[i], msb2_tile_expect[i]);
      abort();
    }
  }
}
int main() {
  SDL_Log("Starting PPU tests.");
  SDL_Log("running `test_gb_tile_to_msb2()`");
  test_gb_tile_to_msb2();
  SDL_Log("PPU tests succeeded.");
  SDL_Quit();
}
#endif
