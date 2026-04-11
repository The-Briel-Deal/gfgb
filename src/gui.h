#ifndef GB_GUI_H
#define GB_GUI_H

#include <SDL3/SDL_events.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gb_state;
typedef struct gb_state gb_state_t;

struct gb_imgui_state {
  uint16_t mem_inspect_addr;
  uint16_t mem_inspect_val;

  // We store the val and addr when read button is pressed so that we can display them to the user ever after the
  // original val changes
  uint16_t mem_inspect_last_read_addr;
  uint8_t  mem_inspect_last_read_val;

  uint16_t mem_inspect_last_write_addr;
  uint8_t  mem_inspect_last_write_val;

  uint16_t breakpoint_addr;
};
typedef struct gb_imgui_state gb_imgui_state_t;

void gb_imgui_init(gb_state_t *gb_state);
void gb_imgui_free(gb_state_t *gb_state);
bool gb_video_handle_sdl_event(struct gb_state *gb_state, SDL_Event *event);
void gb_imgui_render(gb_state_t *gb_state);

#ifdef __cplusplus
}
#endif

#endif // GB_GUI_H
