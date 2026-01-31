#include "ppu.h"
#include "common.h"

#define PIX(x, y) (((tile_in[(y * 2) + 1] >> (7 - x)) & 1) << 1) | (((tile_in[(y * 2) + 0] >> (7 - x)) & 1) << 0)

void gb_tile_to_8bit_indexed(uint8_t *tile_in, uint8_t *tile_out) {
  for (int y = 0; y < 8; y++) {
    uint8_t *line = &tile_out[y * 8];
    for (int x = 0; x < 8; x++) {
      line[x] = PIX(x, y);
    }
  }
}

#undef PIX

inline static uint16_t tile_addr_to_tex_idx(uint16_t tile_addr) {
  // The address this is called with should always cleanly divide by 16.
  assert((tile_addr - GB_TILEDATA_BLOCK0_START) % 16 == 0);
  int tex_index = (tile_addr - GB_TILEDATA_BLOCK0_START) / 16;
  assert(tex_index < DMG_N_TILEDATA_ADDRESSES);
  assert(tex_index >= 0);
  return tex_index;
}

static SDL_Texture *gb_create_tex(struct gb_state *gb_state, uint16_t tile_addr) {
  SDL_Renderer *renderer = gb_state->sdl_renderer;

  uint16_t index = tile_addr_to_tex_idx(tile_addr);

  assert(gb_state->textures[index] == NULL);

  SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_INDEX8, SDL_TEXTUREACCESS_STREAMING, 8, 8);
  if (texture == NULL) {
    SDL_Log("SDL_CreateTexture returned null: %s", SDL_GetError());
    abort();
  }
  gb_state->textures[index] = texture;

  return texture;
}

SDL_Texture *get_texture_for_tile(struct gb_state *gb_state, uint16_t tile_addr) {
  SDL_Texture *texture;

  uint16_t index = tile_addr_to_tex_idx(tile_addr);
  texture = gb_state->textures[index];
  if (texture == NULL) texture = gb_create_tex(gb_state, tile_addr);

  SDL_SetTexturePalette(texture, gb_state->sdl_palette);

  uint8_t *gb_tile = unmap_address(gb_state, tile_addr);
  uint8_t pixels[8 * 8];
  gb_tile_to_8bit_indexed(gb_tile, pixels);

  SDL_UpdateTexture(texture, NULL, pixels, 8);
  return texture;
}

struct oam_entry get_oam_entry(struct gb_state *gb_state, uint8_t index) {
  assert(index < 40);
  struct oam_entry oam_entry = gb_state->oam_entries[index];

#ifdef DEBUG_PRINT_OAM_ENTRIES
  printf("GF_DEBUG: OAM Entry %d\n", index);
  printf("  x_pos = %d\n", oam_entry.x_pos);
  printf("  y_pos = %d\n", oam_entry.y_pos);
  printf("  bank = %d\n", oam_entry.bank);
  printf("  dmg_palette = %d\n", oam_entry.dmg_palette);
  printf("  index = %d\n", oam_entry.index);
  printf("  priority = %d\n", oam_entry.priority);
  printf("  x_flip = %d\n", oam_entry.x_flip);
  printf("  y_flip = %d\n", oam_entry.y_flip);
#endif

  return oam_entry;
}

#ifdef RUN_PPU_TESTS

#include "test_asserts.h"

#include <assert.h>

void test_gb_tile_to_8bit_indexed() {
  uint8_t gb_tile_in[16] = {0};
  uint8_t indexed_8bit_tile_expect[8 * 8] = {0};
  uint8_t indexed_8bit_tile_result[8 * 8] = {0};
  gb_tile_in[0] = 0b1010'1100;
  gb_tile_in[1] = 0b1100'1011;
  indexed_8bit_tile_expect[0] = 0b11;
  indexed_8bit_tile_expect[1] = 0b10;
  indexed_8bit_tile_expect[2] = 0b01;
  indexed_8bit_tile_expect[3] = 0b00;
  indexed_8bit_tile_expect[4] = 0b11;
  indexed_8bit_tile_expect[5] = 0b01;
  indexed_8bit_tile_expect[6] = 0b10;
  indexed_8bit_tile_expect[7] = 0b10;

  gb_tile_to_8bit_indexed(gb_tile_in, indexed_8bit_tile_result);
  for (int i = 0; i < 16; i++) {
    if (indexed_8bit_tile_expect[i] != indexed_8bit_tile_result[i]) {
      SDL_Log("byte i=%d of indexed_8bit result (%.8b) is not equal to result (%.8b)\n", i, indexed_8bit_tile_result[i],
              indexed_8bit_tile_expect[i]);
      abort();
    }
  }
}
int main() {
  SDL_Log("Starting PPU tests.");
  SDL_Log("running `test_gb_tile_to_indexed_8bit()`");
  test_gb_tile_to_8bit_indexed();
  SDL_Log("PPU tests succeeded.");
  SDL_Quit();
}
#endif
