#include <stdint.h>

// See: https://gbdev.io/pandocs/MBC1.html#60007fff--banking-mode-select-write-only
typedef enum mbc1_bank_mode {
  MBC1_BANK_MODE_SIMPLE   = 0,
  MBC1_BANK_MODE_ADVANCED = 1,
} mbc1_bank_mode_t;

typedef struct mbc1_regs {
  bool    ram_enable;
  uint8_t rom_bank;
  union {
    uint8_t ram_bank;
    uint8_t rom_bank_upper;
  };
  mbc1_bank_mode_t banking_mode_select;
} mbc1_regs_t;

typedef struct mbc3_regs {
  uint8_t rtc_sec;
  uint8_t rtc_min;
  uint8_t rtc_hrs;
  uint8_t rtc_day_lower;
  uint8_t rtc_day_upper;

  uint8_t ram_and_rtc_enable;
  uint8_t rom_bank;
  uint8_t ram_or_rtc_bank;
  uint8_t latch_clock_data;
} mbc3_regs_t;

void  gb_alloc_mbc(struct gb_state *gb_state);
void  gb_free_mbc(struct gb_state *gb_state);
void  gb_write_mbc(struct gb_state *gb_state, uint16_t addr, uint8_t val);
void *gb_unmap_mbc_address(struct gb_state *gb_state, uint16_t addr);
