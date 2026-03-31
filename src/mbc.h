#ifndef GB_MBC_H
#define GB_MBC_H

#include <stdint.h>

typedef enum gb_mbc_type {
  GB_NO_MBC,
  GB_MBC1,
  GB_MBC2,
  GB_MBC3,
  GB_MBC5,
  GB_MBC7,
  GB_MMM01,
  GB_HUC1,
  GB_HUC3,
  GB_TPP1,
  GB_CAMERA,

  GB_MBC_UNKNOWN,
} gb_mbc_type_t;

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

typedef struct gb_mbc {
  gb_mbc_type_t type;
  uint16_t      num_rom_banks;
  uint16_t      num_ram_banks;

  // This is a dyn allocated block of memory which is the same length as (num_rom_banks x 16KB), num_rom_banks is the
  // number of rom banks in the rom's header.
  uint32_t rom_size;
  uint8_t *rom_start;
  // Works the same as rom_start except with 8KB banks.
  uint32_t eram_size;
  uint8_t *eram_start;
  union {
    // TODO: break this off into the mbc1 struct once I create it
    mbc1_regs_t mbc1_regs;
  };
} gb_mbc_t;

void  gb_alloc_mbc(gb_mbc_t *mbc);
void  gb_write_mbc(gb_mbc_t *mbc, uint16_t addr, uint8_t val);
void *gb_unmap_mbc_address(gb_mbc_t *mbc, uint16_t addr);

#endif // GB_MBC_H
