from libc.stdint cimport uint16_t, uint8_t

cdef extern from "common.h":
  struct gb_state: pass

  void gb_state_init(gb_state *gb_state)
  gb_state *gb_state_alloc()

cdef extern from "cpu.h":
  struct inst: pass
  enum r8: pass
  enum r16: pass
  enum r16_mem: pass
  enum r16_stk: pass

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
