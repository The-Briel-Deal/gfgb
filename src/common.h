#ifndef GB_COMMON_H
#define GB_COMMON_H

#include <tracy/TracyC.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "disassemble.h"
#include "ppu.h"

#ifdef __cplusplus
#include <utility>

using std::unreachable;
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define GB_malloc(size)           SDL_malloc(size)
#define GB_realloc(mem, size)     SDL_realloc(mem, size)
#define GB_free(mem)              SDL_free(mem)
#define GB_memset(mem, byte, len) SDL_memset(mem, byte, len)

#define GB_assert(expr)           SDL_assert(expr)
enum GB_LogCategory {
  GB_LOG_CATEGORY_DEFAULT = SDL_LOG_CATEGORY_APPLICATION,
  GB_LOG_CATEGORY_PPU     = SDL_LOG_CATEGORY_CUSTOM,
};
#ifdef NDEBUG
#define GB_CheckSDLCall(call) call
#else
#define GB_CheckSDLCall(call)                                                                                          \
  if (!call) {                                                                                                         \
    LogCritical(__FILE__ "@%d: SDL Call '" #call "' failed due to '%s'", __LINE__, SDL_GetError());                    \
  }
#endif

#ifndef GB_LOG_CATEGORY
#define GB_LOG_CATEGORY GB_LOG_CATEGORY_DEFAULT
#endif

#ifdef GFGB_ENABLE_LOGGING
#define LogTrace(msg, ...)    SDL_LogTrace(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogInfo(msg, ...)     SDL_LogInfo(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogDebug(msg, ...)    SDL_LogDebug(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogError(msg, ...)    SDL_LogError(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogCritical(msg, ...) SDL_LogCritical(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#else
#define LogTrace(msg, ...)
#define LogInfo(msg, ...)
#define LogDebug(msg, ...)
#define LogError(msg, ...)
#define LogCritical(msg, ...)
#endif

#define NOT_IMPLEMENTED(msg)                                                                                           \
  {                                                                                                                    \
    LogCritical("This functionality is not yet implemented: %s", msg);                                                 \
    abort();                                                                                                           \
  }

#define ERR(gb_state, msg, ...)                                                                                        \
  {                                                                                                                    \
    LogError(msg, ##__VA_ARGS__);                                                                                      \
    gb_state->err |= true;                                                                                             \
  }

#define KB(n)                      (1024 * n)

#define NIBBLE0(byte)              ((byte & 0xF0) >> 4)
#define NIBBLE1(byte)              ((byte & 0x0F) >> 0)

#define OCTAL0(byte)               ((byte & 0700) >> 6)
#define OCTAL1(byte)               ((byte & 0070) >> 3)
#define OCTAL2(byte)               ((byte & 0007) >> 0)

#define CRUMB0(byte)               ((byte & 0b11000000) >> 6)
#define CRUMB1(byte)               ((byte & 0b00110000) >> 4)
#define CRUMB2(byte)               ((byte & 0b00001100) >> 2)
#define CRUMB3(byte)               ((byte & 0b00000011) >> 0)

#define GB_BG_WIDTH                256
#define GB_BG_HEIGHT               256

#define GB_DISPLAY_WIDTH           160
#define GB_DISPLAY_HEIGHT          144

#define GB_TILEDATA_BLOCK0_START   0x8000
#define GB_TILEDATA_BLOCK1_START   0x8800
#define GB_TILEDATA_BLOCK2_START   0x9000
#define GB_TILEDATA_BLOCK2_END     0x9800

#define GB_TILEMAP_BLOCK0_START    0x9800
#define GB_TILEMAP_BLOCK1_START    0x9C00

#define DMG_PALETTE_N_COLORS       4
#define DMG_N_TILEDATA_ADDRESSES   (128 * 3)
#define DMG_BOOTROM_SIZE           0x100

#define NS_PER_SEC                 (1 * 1000 * 1000 * 1000)

#define DMG_CLOCK_HZ               (1 << 22)

#define DOTS_PER_FRAME             70224

#define ROM0_START                 0x0000
#define ROM0_END                   0x3FFF

#define ROMN_START                 0x4000
#define ROMN_END                   0x7FFF

// VRAM on CGB is switchable across 2 8KB banks, on DMG this is just one 8KB
// block. I won't worry about this until DMG is finished.
#define VRAM_START                 0x8000
#define VRAM_END                   0x9FFF

#define ERAM_START                 0xA000
#define ERAM_END                   0xBFFF

// This is split in two on the CGB and the second half is switchable. I'm just
// worrying about DMG for now.
#define WRAM_START                 0xC000
#define WRAM_END                   0xDFFF

#define ECHO_RAM_START             0xE000
#define ECHO_RAM_END               0xFDFF

#define OAM_START                  0xFE00
#define OAM_END                    0xFE9F

#define IO_REG_START               0xFF00
#define IO_REG_END                 0xFF7F

#define HRAM_START                 0xFF80
#define HRAM_END                   0xFFFE

// This is little endian, so the number is constructed as r2,r1
#define COMBINED_REG(regs, r1, r2) (((uint16_t)regs.r1 << 8) | ((uint16_t)regs.r2 << 0))
#define SET_COMBINED_REG(regs, r1, r2, val)                                                                            \
  {                                                                                                                    \
    regs.r1 = (0xFF00 & val) >> 8;                                                                                     \
    regs.r2 = (0x00FF & val) >> 0;                                                                                     \
  }

#define HBLANK            0
#define VBLANK            1
#define OAM_SCAN          2
#define DRAWING_PIXELS    3

#define TRACY_COLOR_RED   0xff0000
#define TRACY_COLOR_GREEN 0x00ff00
#define TRACY_COLOR_BLUE  0x0000ff
// Simple implementation of a dynamic string.
typedef struct gb_dstr {
  size_t len;
  size_t cap;
  char  *txt;
} gb_dstr_t;

// initialize dynamic string with capacity `cap`
void gb_dstr_init(gb_dstr_t *dstr, size_t cap);
// free dynamic string
void gb_dstr_free(gb_dstr_t *dstr);
// clear dynamic string without freeing or reallocating
void gb_dstr_clear(gb_dstr_t *dstr);
// make sure `n` bytes are available after the len of this str
void gb_dstr_ensure_space(gb_dstr_t *dstr, size_t n);
// append text[len] to gb_dstr
void gb_dstr_append(gb_dstr_t *dstr, char *text, size_t len);

struct gb_ram_banks {
  uint8_t bootrom[DMG_BOOTROM_SIZE];
  uint8_t rom0[KB(16)];
  uint8_t rom1[KB(16)];
  uint8_t wram[KB(8)];
  uint8_t vram[KB(8)];
  uint8_t eram[KB(8)];
  uint8_t hram[0x80];
  uint8_t oam[4 * 40];
};

struct regs {
  uint8_t  a;
  uint8_t  b;
  uint8_t  c;
  uint8_t  d;
  uint8_t  e;
  uint8_t  f;
  uint8_t  h;
  uint8_t  l;
  uint16_t sp;
  uint16_t pc;
  struct io_regs {
    uint8_t joyp;

    uint8_t sc; // serial control

    uint8_t tima; // timer counter
    uint8_t tma;  // timer modulo
    uint8_t tac;  // timer control
    // Sound
    uint8_t nr10;
    uint8_t nr11;
    uint8_t nr12;
    uint8_t nr13;
    uint8_t nr14;

    uint8_t nr21;
    uint8_t nr22;
    uint8_t nr23;
    uint8_t nr24;

    uint8_t nr30;
    uint8_t nr31;
    uint8_t nr32;
    uint8_t nr33;
    uint8_t nr34;

    uint8_t nr41;
    uint8_t nr42;
    uint8_t nr43;
    uint8_t nr44;

    uint8_t nr50;
    uint8_t nr51;
    uint8_t nr52; // sound on/off

    uint8_t ly;
    uint8_t lyc;
    uint8_t stat;

    uint8_t lcdc;
    uint8_t scy;
    uint8_t scx;
    uint8_t bgp;
    uint8_t obp0;
    uint8_t obp1;
    uint8_t wx;
    uint8_t wy;
    uint8_t ie;            // interupt enable
    uint8_t if_;           // interupt flag
    bool    ime;           // interupt master enable
    bool    set_ime_after; // IME is only set after the following instruction.
  } io;
};
typedef struct regs regs_t;
struct gb_state {
  SDL_Window   *sdl_window;
  SDL_Renderer *sdl_renderer;
  SDL_Palette  *sdl_bg_palette;
  SDL_Palette  *sdl_bg_trans0_palette; // this palette is used to draw bits 1-3 on top of priority objects
  SDL_Palette  *sdl_obj_palette_0;
  SDL_Palette  *sdl_obj_palette_1;
  // we draw into these surfaces initially, then we scan our current line and copy it to the screen on hblank
  SDL_Surface *sdl_bg_target;
  SDL_Surface *sdl_win_target;
  SDL_Surface *sdl_obj_target;
  // this is where obj's that have the priority bit set are rendered, these need to be rendered before the bits 1-3 of
  // background and window.
  SDL_Surface *sdl_obj_priority_target;
  SDL_Texture *sdl_composite_target; // this is what all targets are rendered to line by line

  regs_t       regs;

  // Window Related
  bool    wy_cond;
  bool    wx_cond;
  uint8_t win_line_counter;
  bool    win_line_blank;

  bool    halted;
  bool    bootrom_mapped;
  bool    bootrom_has_syms;
  bool    rom_loaded;
  bool    use_flat_ram;
  union {
    struct gb_ram_banks ram;
    uint8_t             flat_ram[KB(64)];
  };
  struct debug_symbol_list syms;
  SDL_Texture             *textures[DMG_N_TILEDATA_ADDRESSES];
  bool                     dirty_textures[DMG_N_TILEDATA_ADDRESSES];

  // this is where all of the oam entries to be drawn on the current line are gathered and ordered during the oam read
  // window
  const struct oam_entry *oam_entries[10];

  FILE                   *serial_port_output;

  // used for getting fps
  uint64_t last_frame_ticks_ns;

  // total m_cycles_elapsed on the cpu
  //
  // This is currently just based on the simple m_cycle timing of each instruction from
  // https://gekkio.fi/files/gb-docs/gbctr.pdf. If I ever want to pass cycle accurate tests i'll need to account for the
  // SM83's overlaping fetch/execute and any other timing idiosyncrasies. For now though just using the simple timings
  // should be enough to make most games run.
  uint64_t m_cycles_elapsed;

  // used for identifying when we are in hblank, and for knowing when we can increment ly.
  uint32_t lcd_x;

  bool     last_stat_interrupt;

  uint8_t  last_mode_handled;

  // the first oam_scan after enabling the PPU still shows as mode 0 despite it scanning oam
  bool first_oam_scan_after_enable;

  // used for updating the timer io regs.
  uint32_t last_timer_sync_m_cycles;

  bool     err;

  // runtime debug toggles
  bool              dbg_clear_composite;
  bool              dbg_hide_bg;
  bool              dbg_hide_win;
  bool              dbg_hide_objs;

  gb_imgui_state_t imgui_state;
};
typedef struct gb_state gb_state_t;

enum io_reg_addr {
  IO_JOYP   = 0xFF00,
  IO_SB     = 0xFF01,
  IO_SC     = 0xFF02,
  IO_TIMA   = 0xFF05,
  IO_TMA    = 0xFF06,
  IO_TAC    = 0xFF07,
  IO_NR10   = 0xFF10,
  IO_NR11   = 0xFF11,
  IO_NR12   = 0xFF12,
  IO_NR13   = 0xFF13,
  IO_NR14   = 0xFF14,
  IO_NR21   = 0xFF16,
  IO_NR22   = 0xFF17,
  IO_NR23   = 0xFF18,
  IO_NR24   = 0xFF19,
  IO_NR30   = 0xFF1A,
  IO_NR31   = 0xFF1B,
  IO_NR32   = 0xFF1C,
  IO_NR33   = 0xFF1D,
  IO_NR34   = 0xFF1E,
  IO_NR41   = 0xFF20,
  IO_NR42   = 0xFF21,
  IO_NR43   = 0xFF22,
  IO_NR44   = 0xFF23,
  IO_NR50   = 0xFF24,
  IO_NR51   = 0xFF25,
  IO_IF     = 0xFF0F,
  IO_IE     = 0xFFFF,
  IO_SND_ON = 0xFF26,
  IO_LCDC   = 0xFF40,
  IO_SCY    = 0xFF42,
  IO_SCX    = 0xFF43,

  IO_WY     = 0xFF4A,
  IO_WX     = 0xFF4B,

  // LCD Status Registers
  IO_LY   = 0xFF44,
  IO_LYC  = 0xFF45,
  IO_STAT = 0xFF41,

  IO_BGP  = 0xFF47,
  IO_OBP0 = 0xFF48,
  IO_OBP1 = 0xFF49,
};

typedef uint16_t io_reg_addr_t;

enum joy_pad_io_reg_bits : uint8_t {
  JOYP_SELECT_D_PAD = 1 << 4,
  // D-Pad Dirs: if JOYP_SELECT_D_PAD is selected (aka is 0)
  JOYP_D_PAD_RIGHT    = 1 << 0,
  JOYP_D_PAD_LEFT     = 1 << 1,
  JOYP_D_PAD_UP       = 1 << 2,
  JOYP_D_PAD_DOWN     = 1 << 3,

  JOYP_SELECT_BUTTONS = 1 << 5,
  // Buttons: if JOYP_SELECT_BUTTONS is selected (aka is 0)
  JOYP_BUTTON_A      = 1 << 0,
  JOYP_BUTTON_B      = 1 << 1,
  JOYP_BUTTON_SELECT = 1 << 2,
  JOYP_BUTTON_START  = 1 << 3,
};

uint64_t         m_cycles(struct gb_state *gb_state);

void             update_timers(struct gb_state *gb_state);

void            *unmap_address(struct gb_state *gb_state, uint16_t addr);

uint8_t          read_mem8(struct gb_state *gb_state, uint16_t addr);
void             write_mem8(struct gb_state *gb_state, uint16_t addr, uint8_t val);

uint16_t         read_mem16(struct gb_state *gb_state, uint16_t addr);
void             write_mem16(struct gb_state *gb_state, uint16_t addr, uint16_t val);

void             gb_state_init(struct gb_state *gb_state);
struct gb_state *gb_state_alloc();
void             gb_state_free(struct gb_state *gb_state);

bool             gb_state_get_err(struct gb_state *gb_state);

// Whether or not to use flat memory, this is currently exclusively used for single step tests where they expect memory
// to be a flat 64KB bank.
void gb_state_use_flat_mem(struct gb_state *gb_state, bool enabled);

// This is in common since I need to also use this for marking textures dirty when they are written to.
inline static uint16_t tile_addr_to_tex_idx(uint16_t tile_addr) {
  int tex_index = (tile_addr - GB_TILEDATA_BLOCK0_START) / 16;
  GB_assert(tex_index < DMG_N_TILEDATA_ADDRESSES);
  GB_assert(tex_index >= 0);
  return tex_index;
}
#ifdef __cplusplus
}
#endif

#endif // GB_COMMON_H
