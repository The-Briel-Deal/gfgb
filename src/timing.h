#ifndef GB_TIMING_H
#define GB_TIMING_H

#include <stdint.h>

typedef struct gb_state gb_state_t;

void gb_handle_div_write(gb_state_t *gb_state);
void gb_spend_mcycles(gb_state_t *gb_state, uint16_t n);

#endif // GB_TIMING_H
