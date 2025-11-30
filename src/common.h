#ifndef GB_COMMON_H
#define GB_COMMON_H

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#define GB_DISPLAY_WIDTH 160
#define GB_DISPLAY_HEIGHT 144
#define COMBINED_REG(regs, r1, r2)                                             \
  ((uint16_t)regs.r1 << 8 | (uint16_t)regs.r2 << 0)
struct gb_state {
  SDL_Window *sdl_window;
  SDL_Renderer *sdl_renderer;
  struct {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t f;
    uint8_t h;
    uint8_t l;
  } regs;
  uint8_t display[GB_DISPLAY_WIDTH][GB_DISPLAY_HEIGHT];
};

static void gb_state_init(struct gb_state *gb_state) {
  gb_state->sdl_window = NULL;
  gb_state->sdl_renderer = NULL;
  SDL_zero(gb_state->display);
  SDL_zero(gb_state->regs);
}

#endif // GB_COMMON_H
