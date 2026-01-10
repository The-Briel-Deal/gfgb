#include "common.h"

#define PIX(x, y)                                                              \
  (((tile_in[(y * 2) + 1] >> (7 - x)) & 1) << 1) |                             \
      (((tile_in[(y * 2) + 0] >> (7 - x)) & 1) << 0)

void gb_tile_to_8bit_indexed(uint8_t *tile_in, uint8_t *tile_out) {
  for (int y = 0; y < 8; y++) {
    uint8_t *line = &tile_out[y * 8];
    for (int x = 0; x < 8; x++) {
      line[x] = PIX(x, y);
    }
  }
}

#undef PIX
static SDL_Texture *gb_create_tex(struct gb_state *gb_state,
                                  uint16_t tile_addr) {
  int index = (tile_addr) / 16;
}

SDL_Texture *get_texture_for_tile(struct gb_state *gb_state,
                                  uint16_t tile_addr) {
  SDL_Renderer *renderer = gb_state->sdl_renderer;
  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8,
                                           SDL_TEXTUREACCESS_STREAMING, 8, 8);
  if (texture == NULL) {
    SDL_Log("SDL_CreateTexture returned null: %s", SDL_GetError());
    abort();
  }
  SDL_SetTexturePalette(texture, gb_state->sdl_palette);

  uint8_t *gb_tile = unmap_address(gb_state, tile_addr);
  uint8_t pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);

  SDL_UpdateTexture(texture, NULL, pixels, 8);
  return texture;
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

  gb_tile_to_8bit_indexed(gb_tile_in, msb2_tile_result);
  for (int i = 0; i < 16; i++) {
    if (msb2_tile_expect[i] != msb2_tile_result[i]) {
      SDL_Log("byte i=%d of msb2 result (%.8b) is not equal to result (%.8b)\n",
              i, msb2_tile_result[i], msb2_tile_expect[i]);
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
