#include "common.h"

void gb_alloc_mbc1(gb_mbc_t *mbc, gb_cart_header_t &header) {

  uint32_t mbc_bytes_required = 0;

  uint32_t rom_banks_size = (mbc->num_rom_banks * (KB(16)));
  mbc_bytes_required += rom_banks_size;
  uint32_t eram_banks_size = (mbc->num_ram_banks * (KB(8)));
  mbc_bytes_required += eram_banks_size;
  // These are both allocated in one call to malloc, the eram block comes directly after the rom block.
  mbc->rom_start = (uint8_t *)GB_malloc(mbc_bytes_required);
  mbc->rom_size  = rom_banks_size;
  GB_assert(mbc->rom_start != NULL);
  mbc->eram_size = eram_banks_size;
  if (header.has_ram) {
    GB_assert(mbc->eram_size != 0);
    mbc->eram_start = &mbc->rom_start[rom_banks_size];
  }

  // The rom_bank is the one field that should default to 1,
  // everything else was initialized to zero in gb_state_init().
  mbc->mbc1_regs.ram_enable          = 0;
  mbc->mbc1_regs.ram_bank            = 0;
  mbc->mbc1_regs.rom_bank            = 1;
  mbc->mbc1_regs.rom_bank_upper      = 0;
  mbc->mbc1_regs.banking_mode_select = MBC1_BANK_MODE_SIMPLE;
}

void gb_alloc_no_mbc(gb_mbc_t *mbc) {
  uint32_t mbc_bytes_required = 0;

  uint32_t rom_banks_size = (2 * (KB(16)));
  mbc_bytes_required += rom_banks_size;
  uint32_t eram_banks_size = (1 * (KB(8)));
  mbc_bytes_required += eram_banks_size;
  // These are both allocated in one call to malloc, the eram block comes directly after the rom block.
  mbc->rom_start = (uint8_t *)GB_malloc(mbc_bytes_required);
  mbc->rom_size  = rom_banks_size;
  GB_assert(mbc->rom_start != NULL);
  mbc->eram_start = &mbc->rom_start[rom_banks_size];
  mbc->eram_size  = eram_banks_size;
}

gb_mbc::gb_mbc(gb_cart_header_t &header) {
  this->type          = header.mbc_type;
  this->num_rom_banks = header.num_rom_banks;
  this->num_ram_banks = header.num_ram_banks;
  this->eram_start    = NULL;
  switch (this->type) {
  case GB_NO_MBC: gb_alloc_no_mbc(this); break;
  case GB_MBC1: gb_alloc_mbc1(this, header); break;
  case GB_MBC2:
  case GB_MBC3:
  case GB_MBC5:
  case GB_MBC7:
  case GB_MMM01:
  case GB_HUC1:
  case GB_HUC3:
  case GB_TPP1:
  case GB_CAMERA:
  case GB_MBC_UNKNOWN: NOT_IMPLEMENTED("Write attempted on MBC that is not yet implemented."); break;
  }
}
gb_mbc::~gb_mbc() {

  GB_free(this->rom_start);
  this->rom_start  = NULL;
  this->eram_start = NULL;
}
static void gb_write_mbc1(gb_mbc_t *mbc, uint16_t addr, uint8_t val) {
  GB_assert(addr < 0x8000);
  // There are 4 unique places in memory that mbc1 receives writes to. Which of these is written to is determined by
  // bits 14 and 13.
  uint8_t      bank_reg  = addr >> 13;
  mbc1_regs_t &mbc1_regs = mbc->mbc1_regs;
  switch (bank_reg) {
  case 0: // 0x0000-0x1FFF
    // It seems like it's unknown why they just check if the lower 4 bits are 0xA.
    mbc1_regs.ram_enable = ((val & 0x0F) == 0x0A);
    break;
  case 1: // 0x2000-0x3FFF
    mbc1_regs.rom_bank = (val & 0b0001'1111);
    // 0 reads as if it is 1 to prevent mapping bank 0 to both areas. This needs to happen before the not needed bits
    // are masked out.
    if (mbc1_regs.rom_bank == 0) mbc1_regs.rom_bank = 1;

    if (mbc1_regs.rom_bank >= mbc->num_rom_banks) {
      mbc1_regs.rom_bank &= mbc->num_rom_banks - 1;
    }
    break;
  case 2: // 0x4000-0x5FFF
    mbc1_regs.rom_bank_upper = (val & 0b11);
    // These should be the same value since they are in an anonymous union. I just gave them two names for clarity.
    assert(mbc1_regs.rom_bank_upper == mbc1_regs.ram_bank);
    break;
  case 3: // 0x6000-0x7FFF
    mbc1_regs.banking_mode_select = (mbc1_bank_mode_t)(val & 0b1);
    break;
  default: unreachable(); break;
  }
}
// Called whenever gb_write_mem is called on ROM.
void gb_mbc::write(uint16_t addr, uint8_t val) {
  switch (this->type) {
  case GB_NO_MBC: break; // TODO: I need to make a write handler for NO_MBC since eram can still be written to.
  case GB_MBC1: gb_write_mbc1(this, addr, val); break;
  case GB_MBC2:
  case GB_MBC3:
  case GB_MBC5:
  case GB_MBC7:
  case GB_MMM01:
  case GB_HUC1:
  case GB_HUC3:
  case GB_TPP1:
  case GB_CAMERA:
  case GB_MBC_UNKNOWN: NOT_IMPLEMENTED("Write attempted on MBC that is not yet implemented."); break;
  }
}

static void *gb_unmap_mbc1_address(gb_mbc_t *mbc, uint16_t addr) {
  if (addr <= ROM0_END) {

    uint8_t bank = 0;
    if (mbc->mbc1_regs.banking_mode_select == MBC1_BANK_MODE_ADVANCED) {
      bank = mbc->mbc1_regs.rom_bank_upper * 0x20;
      bank &= (mbc->num_rom_banks - 1);
    }
    return &mbc->rom_start[(KB(16) * bank) + (addr - ROM0_START)];
  }
  if (addr <= ROMN_END) {
    uint8_t bank = mbc->mbc1_regs.rom_bank;
    bank |= (((mbc->mbc1_regs.rom_bank_upper & 0b11) << 5));
    bank &= (mbc->num_rom_banks - 1);
    return &mbc->rom_start[(KB(16) * bank) + (addr - ROMN_START)];
  }
  if (addr >= ERAM_START && addr <= ERAM_END) {
    if (mbc->mbc1_regs.ram_enable) {
      GB_assert(mbc->num_ram_banks <= 4);
      uint8_t bank = 0;
      if (mbc->mbc1_regs.banking_mode_select == MBC1_BANK_MODE_ADVANCED) {
        bank = mbc->mbc1_regs.ram_bank;
        bank &= (mbc->num_ram_banks - 1);
      }
      return &mbc->eram_start[(KB(8) * bank) + (addr - ERAM_START)];
    }
    LogDebug("MBC1 ERAM Read without ram_enabled set.");
    return NULL;
  }
  LogError("Invalid MBC1 address unmapped $%.4X.", addr);
  return NULL;
}
static void *gb_unmap_no_mbc_address(gb_mbc_t *mbc, uint16_t addr) {
  if (addr <= ROM0_END) {
    return &mbc->rom_start[(KB(16) * 0) + (addr - ROM0_START)];
  }
  if (addr <= ROMN_END) {
    return &mbc->rom_start[(KB(16) * 1) + (addr - ROMN_START)];
  }
  if (addr >= ERAM_START && addr <= ERAM_END) {
    return &mbc->eram_start[(KB(8) * 0) + (addr - ERAM_START)];
  }
  LogError("Invalid NO_MBC address unmapped $%.4X.", addr);
  return NULL;
}
void *gb_mbc::unmap(uint16_t addr) {
  switch (this->type) {
  case GB_NO_MBC: return gb_unmap_no_mbc_address(this, addr);
  case GB_MBC1: return gb_unmap_mbc1_address(this, addr);
  case GB_MBC2:
  case GB_MBC3:
  case GB_MBC5:
  case GB_MBC7:
  case GB_MMM01:
  case GB_HUC1:
  case GB_HUC3:
  case GB_TPP1:
  case GB_CAMERA:
  case GB_MBC_UNKNOWN: NOT_IMPLEMENTED("Unmap attempted on MBC that is not yet implemented."); return NULL;
  default: unreachable();
  }
}
