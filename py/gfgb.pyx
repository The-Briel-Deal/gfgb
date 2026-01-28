import enum
import cython

from cython.cimports import gfgb  # type: ignore


class R8(enum.IntEnum):
  B = 0
  C = 1
  D = 2
  E = 3
  H = 4
  L = 5
  HL_DREF = 6
  A = 7


class R16(enum.IntEnum):
  BC = 0
  DE = 1
  HL = 2
  SP = 3


@cython.cclass
class GB_State:
  _gb_state: cython.pointer[gfgb.gb_state]

  def __cinit__(self):
    self._gb_state = gfgb.gb_state_alloc()
    gfgb.gb_state_init(self._gb_state)

  def __dealloc__(self):
    gfgb.gb_state_free(self._gb_state)

  def set_r8(self, reg: R8, val: cython.uint):
    gfgb.set_r8(self._gb_state, reg, val)

  def get_r8(self, reg: R8) -> cython.uint:
    return gfgb.get_r8(self._gb_state, reg)

  def set_f(self, val: cython.uint):
    gfgb.set_r16_stk(
        self._gb_state, gfgb.r16_stk.R16_STK_AF, (self.get_r8(R8.A) << 8) | val
    )

  def get_f(self) -> cython.uint:
    return gfgb.get_r16_stk(self._gb_state, gfgb.r16_stk.R16_STK_AF) & 0xFF

  def set_pc(self, val: cython.uint):
    gfgb.set_pc(self._gb_state, val)

  def get_pc(self) -> cython.uint:
    return gfgb.get_pc(self._gb_state)

  def set_r16(self, reg: R16, val: cython.uint):
    gfgb.set_r16(self._gb_state, reg, val)

  def get_r16(self, reg: R16) -> cython.uint:
    return gfgb.get_r16(self._gb_state, reg)

  def set_ime(self, val: bool):
    gfgb.set_ime(self._gb_state, val)

  def get_ime(self) -> bool:
    return gfgb.get_ime(self._gb_state)

  def read_mem8(self, addr: cython.uint) -> cython.uint:
    return gfgb.read_mem8(self._gb_state, addr)

  def write_mem8(self, addr: cython.uint, val: cython.uint):
    gfgb.write_mem8(self._gb_state, addr, val)

  def fetch_and_exec(self):
    inst = gfgb.fetch(self._gb_state)
    gfgb.execute(self._gb_state, inst)

  def m_cycles(self):
    gfgb.m_cycles(self._gb_state)

  def get_err(self) -> bool:
    return gfgb.gb_state_get_err(self._gb_state)

  def use_flat_mem(self, enabled: bool):
    gfgb.gb_state_use_flat_mem(self._gb_state, enabled)
