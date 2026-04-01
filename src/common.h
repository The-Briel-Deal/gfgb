#ifndef GB_COMMON_H
#define GB_COMMON_H

// IWYU pragma: begin_exports
#include <tracy/TracyC.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "cpu.h"
#include "disassemble.h"
#include "mbc.h"
#include "ppu.h"

#ifdef __cplusplus

#include <regex>
#include <stack>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using std::unreachable;

#endif // #ifdef __cplusplus

#ifdef __cplusplus
extern "C" {
#endif
// IWYU pragma: end_exports

#define GB_malloc(size)           SDL_malloc(size)
#define GB_realloc(mem, size)     SDL_realloc(mem, size)
#define GB_free(mem)              SDL_free(mem)
#define GB_memset(mem, byte, len) SDL_memset(mem, byte, len)

#define GB_assert(expr) SDL_assert(expr)
enum GB_LogCategory {
  GB_LOG_CATEGORY_DEFAULT = SDL_LOG_CATEGORY_APPLICATION,
  GB_LOG_CATEGORY_PPU     = SDL_LOG_CATEGORY_CUSTOM,
  GB_LOG_CATEGORY_IO_REGS,
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
#define LogTraceCat(cat, msg, ...)    SDL_LogTrace(cat, msg, ##__VA_ARGS__)
#define LogInfoCat(cat, msg, ...)     SDL_LogInfo(cat, msg, ##__VA_ARGS__)
#define LogDebugCat(cat, msg, ...)    SDL_LogDebug(cat, msg, ##__VA_ARGS__)
#define LogWarnCat(cat, msg, ...)     SDL_LogWarn(cat, msg, ##__VA_ARGS__)
#define LogErrorCat(cat, msg, ...)    SDL_LogError(cat, msg, ##__VA_ARGS__)
#define LogCriticalCat(cat, msg, ...) SDL_LogCritical(cat, msg, ##__VA_ARGS__)
#else
#define LogTraceCat(cat, msg, ...)
#define LogInfoCat(cat, msg, ...)
#define LogDebugCat(cat, msg, ...)
#define LogWarnCat(cat, msg, ...)
#define LogErrorCat(cat, msg, ...)
#define LogCriticalCat(cat, msg, ...)
#endif

#define LogTrace(msg, ...)    LogTraceCat(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogInfo(msg, ...)     LogInfoCat(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogDebug(msg, ...)    LogDebugCat(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogWarn(msg, ...)     LogWarnCat(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogError(msg, ...)    LogErrorCat(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)
#define LogCritical(msg, ...) LogCriticalCat(GB_LOG_CATEGORY, msg, ##__VA_ARGS__)

#define NOT_IMPLEMENTED(msg)                                                                                           \
  {                                                                                                                    \
    LogCritical("This functionality is not yet implemented: %s", msg);                                                 \
    abort();                                                                                                           \
  }

#define ERR(gb_state, msg, ...)                                                                                        \
  {                                                                                                                    \
    LogError(msg, ##__VA_ARGS__);                                                                                      \
    if (gb_state->dbg.pause_on_err) {                                                                                  \
      gb_state->dbg.execution_paused = true;                                                                           \
    }                                                                                                                  \
    gb_state->dbg.err = true;                                                                                          \
  }

#define KB(n) (1024 * n)

#define NIBBLE0(byte) ((byte & 0xF0) >> 4)
#define NIBBLE1(byte) ((byte & 0x0F) >> 0)

#define OCTAL0(byte) ((byte & 0700) >> 6)
#define OCTAL1(byte) ((byte & 0070) >> 3)
#define OCTAL2(byte) ((byte & 0007) >> 0)

#define CRUMB0(byte) ((byte & 0b11000000) >> 6)
#define CRUMB1(byte) ((byte & 0b00110000) >> 4)
#define CRUMB2(byte) ((byte & 0b00001100) >> 2)
#define CRUMB3(byte) ((byte & 0b00000011) >> 0)

#define GB_BG_WIDTH  256
#define GB_BG_HEIGHT 256

#define GB_DISPLAY_WIDTH  160
#define GB_DISPLAY_HEIGHT 144

#define GB_TILEDATA_BLOCK0_START 0x8000
#define GB_TILEDATA_BLOCK1_START 0x8800
#define GB_TILEDATA_BLOCK2_START 0x9000
#define GB_TILEDATA_BLOCK2_END   0x9800

#define GB_TILEMAP_BLOCK0_START 0x9800
#define GB_TILEMAP_BLOCK1_START 0x9C00

#define DMG_PALETTE_N_COLORS     4
#define DMG_N_TILEDATA_ADDRESSES (128 * 3)
#define DMG_BOOTROM_SIZE         0x100

#define NS_PER_SEC (1 * 1000 * 1000 * 1000)

#define DMG_CLOCK_HZ (1 << 22)

#define DOTS_PER_FRAME 70224

#define ROM0_START 0x0000
#define ROM0_END   0x3FFF

#define ROMN_START 0x4000
#define ROMN_END   0x7FFF

// VRAM on CGB is switchable across 2 8KB banks, on DMG this is just one 8KB
// block. I won't worry about this until DMG is finished.
#define VRAM_START 0x8000
#define VRAM_END   0x9FFF

#define ERAM_START 0xA000
#define ERAM_END   0xBFFF

// This is split in two on the CGB and the second half is switchable. I'm just
// worrying about DMG for now.
#define WRAM_START 0xC000
#define WRAM_END   0xDFFF

#define ECHO_RAM_START 0xE000
#define ECHO_RAM_END   0xFDFF

#define OAM_START 0xFE00
#define OAM_END   0xFE9F

#define IO_REG_START 0xFF00
#define IO_REG_END   0xFF7F

#define HRAM_START 0xFF80
#define HRAM_END   0xFFFE

// This is little endian, so the number is constructed as r2,r1
#define COMBINED_REG(regs, r1, r2) (((uint16_t)regs.r1 << 8) | ((uint16_t)regs.r2 << 0))
#define SET_COMBINED_REG(regs, r1, r2, val)                                                                            \
  {                                                                                                                    \
    regs.r1 = (0xFF00 & val) >> 8;                                                                                     \
    regs.r2 = (0x00FF & val) >> 0;                                                                                     \
  }

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

typedef struct gb_mem {
  uint8_t  bootrom[DMG_BOOTROM_SIZE];
  uint8_t  wram[KB(8)];
  uint8_t  vram[KB(8)];
  uint8_t  hram[0x80];
  uint8_t  oam[4 * 40];
  gb_mbc_t mbc;
} gb_mem_t;

// true if pressed down
typedef struct gb_internal_joy_pad_state {
  bool dpad_right;
  bool dpad_left;
  bool dpad_up;
  bool dpad_down;

  bool button_a;
  bool button_b;
  bool button_select;
  bool button_start;
} gb_internal_joy_pad_state_t;

typedef struct io_regs {
  uint8_t joyp;

  uint8_t sb; // serial transfer data (currently unused)
  uint8_t sc; // serial transfer control (currently unused)

  // This is technically the system clock which is essentially the full 16 bit version of DIV, the DIV register just
  // returns the most significant 8 bits.
  uint8_t div;  // divider register
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
  uint8_t ie;  // interupt enable
  uint8_t if_; // interupt flag
  uint8_t dma;
  bool    ime;           // interupt master enable
  bool    set_ime_after; // IME is only set after the following instruction.
  bool    bank;          // True at start if bootrom is mapped, then once 0xFF50 is written to it becomes false.
} io_regs_t;

typedef struct regs {
  uint8_t   a;
  uint8_t   b;
  uint8_t   c;
  uint8_t   d;
  uint8_t   e;
  uint8_t   f;
  uint8_t   h;
  uint8_t   l;
  uint16_t  sp;
  uint16_t  pc;
  io_regs_t io;
} regs_t;

typedef struct gb_breakpoint {
  uint16_t addr;
  // TODO: We will want to also allow breakpoints to only be on a specific bank. So I should add an optional bank field.
  bool enable;
} gb_breakpoint_t;

typedef enum stack_entry_type {
  CALL_RET,
  PUSH_VAL,
} stack_entry_type_t;

typedef struct call_ret_metadata {
  debug_symbol_t *callee_symbol;
  debug_symbol_t *caller_symbol;
} call_ret_metadata_t;

typedef struct push_val_metadata {
  uint16_t push_inst_addr;
} push_val_metadata_t;

typedef struct stack_entry {
  stack_entry_type_t type;
  uint16_t           val;
  union {
    call_ret_metadata_t call_ret;
    push_val_metadata_t push_val;
  };
} stack_entry_t;

typedef struct gb_cart_header {
  gb_mbc_type_t mbc_type;
  bool          has_ram;
  bool          has_battery;
  bool          has_rtc;
  bool          has_rumble;
  uint16_t      num_rom_banks;
  uint16_t      num_ram_banks;
} gb_cart_header_t;

typedef struct gb_saved_state {
  gb_cart_header_t header;
  regs_t           regs;
  union {
    gb_mem_t mem;
    uint8_t  flat_ram[KB(64)];
  };
  bool halted;

  // total m_cycles_elapsed on the cpu
  //
  // This is currently just based on the simple m_cycle timing of each instruction from
  // https://gekkio.fi/files/gb-docs/gbctr.pdf. If I ever want to pass cycle accurate tests i'll need to account for the
  // SM83's overlaping fetch/execute and any other timing idiosyncrasies. For now though just using the simple timings
  // should be enough to make most games run.
  uint64_t m_cycles_elapsed;

  uint64_t last_timer_sync_m_cycles;
  bool     last_tima_bit;
  bool     last_stat_bit;

  bool    wy_cond;
  bool    wx_cond;
  uint8_t win_line_counter;
  bool    win_line_blank;

  // TODO: Since this will be included in save state, we should probably make this a list of oam_entries instead of
  // ptr's to oam_entries.
  //
  // this is where oam entries to be drawn on the current line are gathered during oam read
  const oam_entry_t *oam_entries[10];
} gb_saved_state_t;

typedef struct gb_video_state {
  bool initialized; // We don't initalize video in the case of `disasm` subcmd and when an error occurs in
                    // argparsing. So we want to make sure we don't try to free what was never created.
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
  // We draw everything line by line into the back, then on v-sync we swap the pointers
  SDL_Texture *sdl_composite_target_front;
  SDL_Texture *sdl_composite_target_back;
  SDL_Texture *textures[DMG_N_TILEDATA_ADDRESSES];
  bool         dirty_textures[DMG_N_TILEDATA_ADDRESSES];
  // the first oam_scan after enabling the PPU still shows as mode 0 despite it scanning oam
  bool first_oam_scan_after_enable;
  bool oam_dma_start;

  uint64_t frame_num;
} gb_video_state_t;

typedef struct gb_timing_state {
  uint64_t ns_elapsed_while_running;
  uint64_t ns_elapsed_total;
  uint16_t sysclk; // (T-Cycles) The heart of the timing system, div is just the most significant 8 bits of this.
  uint32_t ppu_frame_dots; // A value in dots between 0 and 70,224 (which is the total number of dots in a frame).
} gb_timing_state_t;

// runtime debug toggles
typedef struct gb_dbg_state {
  bool                err;
  bool                clear_composite;
  bool                rom_loaded;
  bool                bootrom_has_syms;
  bool                use_flat_ram;
  bool                hide_bg;
  bool                hide_win;
  bool                hide_objs;
  bool                pause_on_err;
  bool                execution_paused;
  bool                fs_dockspace;
  bool                headless_mode; // Whether or not there is an actual window to present to.
  bool                test_mode;     // If enabled then use serial_port output to look for a pass/fail string
  bool                trace_exec;
  float               speed_factor;
  uint32_t            step_inst_count; // the number of instructions to run until breaking
  char                test_mode_pass_regex[16];
  char                test_mode_fail_regex[16];
  uint64_t            ns_elapsed_last_gb_vsync; // Used for getting the frametime/fps
  uint64_t            ns_last_frametime;
  FILE               *serial_port_output_file;
  FILE               *trace_exec_fout;
  debug_symbol_list_t syms;
} gb_dbg_state_t;

// TODO: finish moving the rest of these fields into the appropriate nested structs.
typedef struct gb_state {
  gb_saved_state_t            saved;
  gb_dbg_state_t              dbg;
  gb_imgui_state_t            imgui;
  gb_internal_joy_pad_state_t joy_pad;
  gb_video_state_t            video;
  gb_timing_state_t           timing;

  // TODO: This state needs to be at the end of the struct so that this doesn't break layout for C FFI. I should
  // probably just have a private void ptr on the state sections which use cpp structs.
#ifdef __cplusplus
  std::string            *serial_port_output_string;
  std::basic_regex<char> *compiled_pass_regex;
  std::basic_regex<char> *compiled_fail_regex;
  // The shadow stack is used for debugging to keep track of stack entries along with metadata so that we can display it
  // in the debug UI.
  std::stack<stack_entry_t>    *shadow_stack;
  std::vector<gb_breakpoint_t> *breakpoints;
#endif
} gb_state_t;

#ifdef __cplusplus
static_assert(std::is_standard_layout<gb_state>());
#endif

bool gb_load_rom(struct gb_state *gb_state, const char *rom_name, const char *bootrom_name, const char *sym_name);
// Call with bootrom_name = NULL to use dmg0 as the default.
void gb_state_load_bootrom(gb_state_t *gb_state, const char *bootrom_name);

// See the following wikipedia page on X Macro's if you want to understand this idiom. I don't love doing this but I
// need a good way to display all IO registers in the ImGui debug UI, and I don't want to have to create a seperate
// lookup table for io reg names.
//
// https://en.wikipedia.org/wiki/X_macro
#define LIST_OF_IO_REGS                                                                                                \
  X(IO_JOYP, 0xFF00)                                                                                                   \
  X(IO_SB, 0xFF01)                                                                                                     \
  X(IO_SC, 0xFF02)                                                                                                     \
  X(IO_DIV, 0xFF04)                                                                                                    \
  X(IO_TIMA, 0xFF05)                                                                                                   \
  X(IO_TMA, 0xFF06)                                                                                                    \
  X(IO_TAC, 0xFF07)                                                                                                    \
  X(IO_NR10, 0xFF10)                                                                                                   \
  X(IO_NR11, 0xFF11)                                                                                                   \
  X(IO_NR12, 0xFF12)                                                                                                   \
  X(IO_NR13, 0xFF13)                                                                                                   \
  X(IO_NR14, 0xFF14)                                                                                                   \
  X(IO_NR21, 0xFF16)                                                                                                   \
  X(IO_NR22, 0xFF17)                                                                                                   \
  X(IO_NR23, 0xFF18)                                                                                                   \
  X(IO_NR24, 0xFF19)                                                                                                   \
  X(IO_NR30, 0xFF1A)                                                                                                   \
  X(IO_NR31, 0xFF1B)                                                                                                   \
  X(IO_NR32, 0xFF1C)                                                                                                   \
  X(IO_NR33, 0xFF1D)                                                                                                   \
  X(IO_NR34, 0xFF1E)                                                                                                   \
  X(IO_NR41, 0xFF20)                                                                                                   \
  X(IO_NR42, 0xFF21)                                                                                                   \
  X(IO_NR43, 0xFF22)                                                                                                   \
  X(IO_NR44, 0xFF23)                                                                                                   \
  X(IO_NR50, 0xFF24)                                                                                                   \
  X(IO_NR51, 0xFF25)                                                                                                   \
  X(IO_IF, 0xFF0F)                                                                                                     \
  X(IO_IE, 0xFFFF)                                                                                                     \
  X(IO_SND_ON, 0xFF26)                                                                                                 \
  X(IO_LCDC, 0xFF40)                                                                                                   \
  X(IO_SCY, 0xFF42)                                                                                                    \
  X(IO_SCX, 0xFF43)                                                                                                    \
  X(IO_WY, 0xFF4A)                                                                                                     \
  X(IO_WX, 0xFF4B)                                                                                                     \
  X(IO_LY, 0xFF44)                                                                                                     \
  X(IO_LYC, 0xFF45)                                                                                                    \
  X(IO_DMA, 0xFF46)                                                                                                    \
  X(IO_STAT, 0xFF41)                                                                                                   \
  X(IO_BGP, 0xFF47)                                                                                                    \
  X(IO_OBP0, 0xFF48)                                                                                                   \
  X(IO_OBP1, 0xFF49)                                                                                                   \
  X(IO_BANK, 0xFF50)

enum io_reg_addr {
#define X(name, addr) name = addr,
  LIST_OF_IO_REGS
#undef X
};
typedef uint16_t io_reg_addr_t;

const io_reg_addr_t io_regs[] = {
#define X(name, _) name,
    LIST_OF_IO_REGS
#undef X
};

inline static const char *gb_io_reg_name(io_reg_addr_t io_reg) {
  switch (io_reg) {
#define X(name, _)                                                                                                     \
  case name: return #name;
    LIST_OF_IO_REGS
#undef X
  default: unreachable();
  }
}

enum joy_pad_io_reg_bits : uint8_t {
  JOYP_SELECT_D_PAD = 1 << 4,
  // D-Pad Dirs: if JOYP_SELECT_D_PAD is selected (aka is 0)
  JOYP_D_PAD_RIGHT = 1 << 0,
  JOYP_D_PAD_LEFT  = 1 << 1,
  JOYP_D_PAD_UP    = 1 << 2,
  JOYP_D_PAD_DOWN  = 1 << 3,

  JOYP_SELECT_BUTTONS = 1 << 5,
  // Buttons: if JOYP_SELECT_BUTTONS is selected (aka is 0)
  JOYP_BUTTON_A      = 1 << 0,
  JOYP_BUTTON_B      = 1 << 1,
  JOYP_BUTTON_SELECT = 1 << 2,
  JOYP_BUTTON_START  = 1 << 3,
};

// Note: I took and modified this from SameBoy's MBC lookup table.

gb_cart_header_t gb_parse_cart_header(uint8_t header[0x50]);

uint64_t gb_m_cycles(gb_state_t *gb_state);

void *gb_unmap_address(gb_state_t *gb_state, uint16_t addr);

uint8_t gb_read_mem(gb_state_t *gb_state, uint16_t addr);
void    gb_write_mem(gb_state_t *gb_state, uint16_t addr, uint8_t val);

void        gb_state_init(gb_state_t *gb_state);
void        gb_state_reset(gb_state_t *gb_state);
gb_state_t *gb_state_alloc();
void        gb_state_free(gb_state_t *gb_state);

bool gb_state_get_err(gb_state_t *gb_state);

// Whether or not to use flat memory, this is currently exclusively used for single step tests where they expect memory
// to be a flat 64KB bank.
void gb_state_use_flat_mem(gb_state_t *gb_state, bool enabled);

// This is in common since I need to also use this for marking textures dirty when they are written to.
inline static uint16_t gb_tile_addr_to_tex_idx(uint16_t tile_addr) {
  int tex_index = (tile_addr - GB_TILEDATA_BLOCK0_START) / 16;
  GB_assert(tex_index < DMG_N_TILEDATA_ADDRESSES);
  GB_assert(tex_index >= 0);
  return tex_index;
}
#ifdef __cplusplus
}
#endif

#endif // GB_COMMON_H
