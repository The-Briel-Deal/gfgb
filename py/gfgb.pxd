from libc.stdint cimport uint16_t, uint8_t

cdef extern from "common.h":
  struct gb_state: pass

  void gb_state_init(gb_state *gb_state)
  gb_state *gb_state_alloc()

cdef extern from "cpu.h":
  struct inst: pass
  enum r8:
    R8_B = 0,
    R8_C = 1,
    R8_D = 2,
    R8_E = 3,
    R8_H = 4,
    R8_L = 5,
    R8_HL_DREF = 6,
    R8_A = 7,

  enum r16:
    R16_BC = 0,
    R16_DE = 1,
    R16_HL = 2,
    R16_SP = 3,

  enum r16_mem:
    R16_MEM_BC = 0,
    R16_MEM_DE = 1,
    R16_MEM_HLI = 2,
    R16_MEM_HLD = 3,

  enum r16_stk:
    R16_STK_BC = 0,
    R16_STK_DE = 1,
    R16_STK_HL = 2,
    R16_STK_AF = 3,

  uint8_t get_r8(gb_state *gb_state, r8 r8)
  void set_r8(gb_state *gb_state, r8 r8, uint8_t val)

  uint16_t get_r16(gb_state *gb_state, r16 r16)
  void set_r16(gb_state *gb_state, r16 r16, uint16_t val)

  uint16_t get_r16_mem(gb_state *gb_state, r16_mem r16_mem)
  void set_r16_mem(gb_state *gb_state, r16_mem r16_mem, uint8_t val)

  uint16_t get_r16_stk(gb_state *gb_state, r16_stk r16_stk)
  void set_r16_stk(gb_state *gb_state, r16_stk r16_stk, uint16_t val)

  inst fetch(gb_state *gb_state)
  void execute(gb_state *gb_state, inst inst)
