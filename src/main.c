#include "common.h"

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  struct gb_state *gb_state = SDL_malloc(sizeof(struct gb_state));
  *appstate = gb_state;
  SDL_assert(appstate != NULL);
  gb_state_init(*appstate);
  SDL_SetAppMetadata("Example Renderer Clear", "1.0",
                     "com.example.renderer-clear");

  if (!SDL_Init(SDL_INIT_VIDEO)) {
    SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }

  if (!SDL_CreateWindowAndRenderer("examples/renderer/clear", 1600, 1440,
                                   SDL_WINDOW_RESIZABLE, &gb_state->sdl_window,
                                   &gb_state->sdl_renderer)) {
    SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
    return SDL_APP_FAILURE;
  }
  SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, 1600, 1440,
                                   SDL_LOGICAL_PRESENTATION_LETTERBOX);

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event) {
  struct gb_state *gb_state = appstate;
  switch (event->type) {
  case SDL_EVENT_QUIT: return SDL_APP_SUCCESS;
  case SDL_EVENT_WINDOW_RESIZED:
    SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer,
                                     event->window.data1, event->window.data2,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
    break;
  }

  return SDL_APP_CONTINUE; /* carry on with the program! */
}

void gb_draw(struct gb_state *gb_state) {
  SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, 0.0, 0.0, 0.0,
                              SDL_ALPHA_OPAQUE_FLOAT);

  /* clear the window to the draw color. */
  SDL_RenderClear(gb_state->sdl_renderer);
  for (int y = 0; y < GB_DISPLAY_HEIGHT; y++) {
    for (int x = 0; x < GB_DISPLAY_WIDTH; x++) {
      uint8_t pixel = gb_state->display[x][y];
      // The original gameboy had 4 shades of grey, these are represented by
      // 0,1,2,3. Anything greater is invalid.
      SDL_assert(pixel < 4);

      float grey_shade = (float)pixel / 3.0;

      SDL_SetRenderDrawColorFloat(gb_state->sdl_renderer, grey_shade,
                                  grey_shade, grey_shade,
                                  SDL_ALPHA_OPAQUE_FLOAT);
      int w, h;
      SDL_GetRenderLogicalPresentation(gb_state->sdl_renderer, &w, &h, NULL);
      float pixel_w = (float)w / (float)GB_DISPLAY_WIDTH;
      float pixel_h = (float)h / (float)GB_DISPLAY_HEIGHT;

      struct SDL_FRect rect = {
          .x = pixel_w * x, .y = pixel_h * y, .w = pixel_w, .h = pixel_h};
      SDL_RenderFillRect(gb_state->sdl_renderer, &rect);
    }
  }
  /* put the newly-cleared rendering on the screen. */
  SDL_RenderPresent(gb_state->sdl_renderer);
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate) {
  struct gb_state *gb_state = appstate;
  gb_draw(gb_state);
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  /* SDL will clean up the window/renderer for us. */
  SDL_free(appstate);
}
