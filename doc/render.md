# The Rendering Process

My goal with the rendering process in this emulator is to keep the internal ppu
display rendering seperate from the actual SDL window rendering. The purpose of
this is to make it so that we can keep rendering emulator UI elements, even if
the gameboy lcd is disabled.

To do this we have two SDL textures in gb_state:
```c
struct gb_state {
  ...
  SDL_Texture *sdl_composite_target_front;
  SDL_Texture *sdl_composite_target_back; 
  ...
};
```

We render line by line on H-Blank into `sdl_composite_target_back`, then on
V-Blank I swap these textures. That means that we know that
`sdl_composite_target_front` always contains a complete frame for rendering to
the window.

To make sure that the emulator is always running at the appropriate speed we
use `SDL_GetTicksNS()` to make sure that we are only running 2^20 (1,048,576)
m_cycles per second. Since SDL can give us ticks in Nanosecond precision, it's
more useful to think of this as, for every 1 nanosecond we want to run
0.001048576 m-cycles. So for every App Iteration, we keep executing until
`((ticks_ns * 0.001048576) > m_cycles_elapsed)` a.k.a `(ticks_ns >
(m_cycles_elapsed / 0.001048576))` a.k.a `(ticks_ns > (m_cycles_elapsed *
953.6743164062499))` (I think I can round up that constant to 954 so that I can
cleanly work with 64 bit unsigned integers).
