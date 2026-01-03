#include "common.h"
#include "cpu.h"
#include "disassemble.h"

#include <SDL3/SDL_init.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define SDL_MAIN_USE_CALLBACKS 1 /* use the callbacks instead of main() */
#include <SDL3/SDL_main.h>

#include <getopt.h>

enum run_mode {
  UNSET = 0,
  EXECUTE,
  DISASSEMBLE,
};

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
  enum run_mode run_mode = UNSET;

  int c;
  char *filename = NULL;
  char *symbol_filename = NULL;

  while ((c = getopt(argc, argv, "edf:s:")) != -1)
    switch (c) {
    case 'e':
      if (run_mode != UNSET) {
        fprintf(stderr,
                "Option `e` and `d` specified, these are mutually exclusive\n");
        return 1;
      }
      run_mode = EXECUTE;
      break;
    case 'd':
      if (run_mode != UNSET) {
        fprintf(stderr,
                "Option `e` and `d` specified, these are mutually exclusive\n");
        return 1;
      }
      run_mode = DISASSEMBLE;
      break;
    case 'f': filename = optarg; break;
    case 's': symbol_filename = optarg; break;
    case '?':
      if (optopt == 'f')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      if (optopt == 's')
        fprintf(stderr, "Option -%c requires an argument.\n", optopt);
      else if (isprint(optopt))
        fprintf(stderr, "Unknown option `-%c'.\n", optopt);
      else
        fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
      return 1;
    default: abort();
    }

  switch (run_mode) {
  case EXECUTE: {
    FILE *f;
    int err;
    uint8_t bytes[KB(16)];
    int bytes_len;
    struct gb_state *gb_state;

    f = fopen(filename, "r");

    bytes_len = fread(bytes, sizeof(uint8_t), KB(16), f);
    if ((err = ferror(f))) {
      SDL_Log("Error when reading rom file: %d", err);
      return SDL_APP_FAILURE;
    }
    fclose(f);

    gb_state = SDL_malloc(sizeof(struct gb_state));
    *appstate = gb_state;
    SDL_assert(appstate != NULL);
    gb_state_init(*appstate);
    SDL_SetAppMetadata("GF-GB", "0.0.1", "com.gf.gameboy-emu");
    memcpy(gb_state->rom0, bytes, bytes_len);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
      SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer(
            "examples/renderer/clear", 1600, 1440, SDL_WINDOW_RESIZABLE,
            &gb_state->sdl_window, &gb_state->sdl_renderer)) {
      SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
      return SDL_APP_FAILURE;
    }
    SDL_SetRenderLogicalPresentation(gb_state->sdl_renderer, 1600, 1440,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    return SDL_APP_CONTINUE; /* carry on with the program! */
  };
  case DISASSEMBLE: {
    FILE *f;
    int err;

    f = fopen(filename, "r");
    uint8_t bytes[KB(16)];

    int len = fread(bytes, sizeof(uint8_t), KB(16), f);
    if ((err = ferror(f))) {
      SDL_Log("Error when reading rom file: %d", err);
      return SDL_APP_FAILURE;
    }
    fclose(f);

    if (symbol_filename != NULL) {
      struct debug_symbol_list syms;
      alloc_symbol_list(&syms);
      f = fopen(symbol_filename, "r");
      parse_syms(&syms, f);
      if ((err = ferror(f))) {
        SDL_Log("Error when reading symbol file: %d", err);
        return SDL_APP_FAILURE;
      }
      fclose(f);
      disassemble_rom_with_sym(stdout, bytes, len, &syms);
      free_symbol_list(&syms);
    } else {
      disassemble_rom(stdout, bytes, len);
    }

    return SDL_APP_SUCCESS;
  }
  case UNSET:
    fprintf(stderr, "Run Mode unset, please specify either `-e` to execute or "
                    "`-d` to disassemble.\n");
    return SDL_APP_FAILURE;
  default: return SDL_APP_FAILURE;
  }
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
#ifdef PRINT_INST_DURING_EXEC
  printf("0x%.4x: ", gb_state->regs.pc);
#endif
  struct inst inst = fetch(gb_state);
#ifdef PRINT_INST_DURING_EXEC
  print_inst(stdout, inst);
#endif
  execute(gb_state, inst);

  gb_draw(gb_state);
  return SDL_APP_CONTINUE; /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result) {
  (void)result;
  /* SDL will clean up the window/renderer for us. */
  SDL_free(appstate);
}
