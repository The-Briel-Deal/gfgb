#include "common.h"
#include "test_common.h"

TEST_CASE("Fetch CPU Instruction", "[CPU]") {
  struct gb_state gb_state;
  struct inst     inst;

  gb_state_init(&gb_state);
  gb_state.saved.header.mbc_type = GB_NO_MBC;
  gb_alloc_mbc(&gb_state.saved.mem.mbc, &gb_state.saved.header);
  gb_state.saved.regs.pc = 0x0100;

  gb_state.saved.mem.mbc.rom_start[0x100] = 0b00100001;
  gb_state.saved.mem.mbc.rom_start[0x101] = 0xCD;
  gb_state.saved.mem.mbc.rom_start[0x102] = 0xAB;
  inst                                = fetch(&gb_state);
  CHECK(inst.type == LD);
  CHECK(inst.p1.type == R16);
  CHECK(inst.p1.r16 == R16_HL);
  CHECK(inst.p2.type == IMM16);
  CHECK(inst.p2.imm16 == 0xABCD);

  gb_state.saved.mem.mbc.rom_start[0x103] = 0b00010010;
  inst                                = fetch(&gb_state);
  CHECK(inst.type == LD);
  CHECK(inst.p1.type == R16_MEM);
  CHECK(inst.p1.r16 == R16_DE);
  CHECK(inst.p2.type == R8);
  CHECK(inst.p2.r8 == R8_A);

  gb_state.saved.mem.mbc.rom_start[0x104] = 0b00011010;
  inst                                = fetch(&gb_state);
  CHECK(inst.type == LD);
  CHECK(inst.p1.type == R8);
  CHECK(inst.p1.r8 == R8_A);
  CHECK(inst.p2.type == R16_MEM);
  CHECK(inst.p2.r16 == R16_DE);

  gb_state.saved.mem.mbc.rom_start[0x105] = 0b00001000;
  gb_state.saved.mem.mbc.rom_start[0x106] = 0x34;
  gb_state.saved.mem.mbc.rom_start[0x107] = 0x12;
  inst                                = fetch(&gb_state);
  CHECK(inst.type == LD);
  CHECK(inst.p1.type == IMM16_MEM);
  CHECK(inst.p1.imm16 == 0x1234);
  CHECK(inst.p2.type == R16);
  CHECK(inst.p2.r16 == R16_SP);
}

TEST_CASE("Fetch CPU Execute LD", "[CPU]") {
  gb_state_t gb_state;
  gb_state_init(&gb_state);
  inst_t inst;

  // Load IMM16 into reg BC
  inst = inst_t{
      .type = LD,
      .p1   = R16_PARAM(R16_BC),
      .p2   = IMM16_PARAM(452),
  };
  execute(&gb_state, inst);
  REQUIRE(get_r16(&gb_state, R16_BC) == 452);

  // Load reg A into addr in reg BC
  inst = inst_t{
      .type = LD,
      .p1   = R16_MEM_PARAM(R16_MEM_BC),
      .p2   = R8_PARAM(R8_A),
  };
  set_r16(&gb_state, R16_BC, 0xC000);
  gb_state.saved.regs.a = 42;
  execute(&gb_state, inst);
  REQUIRE(gb_read_mem(&gb_state, 0xC000) == 42);

  // Load contents of addr in reg BC into reg A
  inst = inst_t{
      .type = LD,
      .p1   = R8_PARAM(R8_A),
      .p2   = R16_MEM_PARAM(R16_MEM_BC),
  };
  gb_write_mem(&gb_state, 0xC000, 134);
  execute(&gb_state, inst);
  REQUIRE(gb_state.saved.regs.a == 134);

  // Load contents of addr in reg HL into reg A and increment the pointer.
  inst = inst_t{
      .type = LD,
      .p1   = R8_PARAM(R8_A),
      .p2   = R16_MEM_PARAM(R16_MEM_HLI),
  };
  set_r16(&gb_state, R16_HL, 0xC000);
  gb_write_mem(&gb_state, 0xC000, 134);
  execute(&gb_state, inst);
  REQUIRE(get_r16(&gb_state, R16_HL) == 0xC001);
  REQUIRE(gb_state.saved.regs.a == 134);
  // Then load contents of reg A into the addr in reg HL.
  inst = inst_t{
      .type = LD,
      .p1   = R16_MEM_PARAM(R16_MEM_HLD),
      .p2   = R8_PARAM(R8_A),
  };
  set_r8(&gb_state, R8_A, 21);
  execute(&gb_state, inst);
  REQUIRE(gb_read_mem(&gb_state, 0xC001) == 21);
  REQUIRE(get_r16(&gb_state, R16_HL) == 0xC000);

  // Load stack pointer into addr at IMM16
  inst = inst_t{
      .type = LD,
      .p1   = IMM16_MEM_PARAM(0xC010),
      .p2   = R16_PARAM(R16_SP),
  };
  set_r16(&gb_state, R16_SP, 0xD123);
  execute(&gb_state, inst);
  REQUIRE(gb_state.saved.regs.sp == 0xD123);
  REQUIRE((gb_read_mem(&gb_state, 0xC010) | (gb_read_mem(&gb_state, 0xC011) << 8)) == 0xD123);
}

TEST_CASE("Stack Operations", "[CPU]") {
  gb_state_t gb_state;
  gb_state_init(&gb_state);

  push16(&gb_state, 0x1234);
  assert_eq(gb_state.saved.regs.sp, 0xDFFE);
  // 16 bit vals on the stack should be little endian so that they can be read
  // like 16 bit values anywhere else in memory.
  assert_eq(gb_read_mem(&gb_state, 0xDFFF), 0x12);
  assert_eq(gb_read_mem(&gb_state, 0xDFFE), 0x34);

  assert_eq(pop16(&gb_state), 0x1234);
}

TEST_CASE("Call and Return", "[CPU]") {
  gb_state_t gb_state;
  gb_state_init(&gb_state);
  assert_eq(gb_state.saved.regs.sp, 0xE000);
  gb_state.saved.regs.pc = 0x0190;
  execute(&gb_state, inst_t{.type = CALL, .p1 = IMM16_PARAM(0x0210), .p2 = VOID_PARAM});
  assert_eq(gb_state.saved.regs.sp, 0xDFFE);
  assert_eq(gb_state.saved.regs.pc, 0x0210);
  assert_eq((gb_read_mem(&gb_state, 0xDFFE) | (gb_read_mem(&gb_state, 0xDFFF) << 8)), 0x0190);
  execute(&gb_state, inst_t{.type = RET, .p1 = VOID_PARAM, .p2 = VOID_PARAM});
  assert_eq(gb_state.saved.regs.sp, 0xE000);
  assert_eq(gb_state.saved.regs.pc, 0x0190);
}
